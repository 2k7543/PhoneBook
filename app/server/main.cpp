#include "service/PhoneBookRuntime.h"
#include "service/ServiceException.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <openssl/rand.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace {
using Json = nlohmann::json;
using namespace std::chrono_literals;

struct WebSession {
    RuntimeSession user;
    std::chrono::system_clock::time_point expiresAt;
};

PhoneBookRuntime* runtime = nullptr;
std::mutex runtimeMutex;
std::mutex sessionsMutex;
std::unordered_map<std::string, WebSession> sessions;

constexpr auto kSessionLifetime = std::chrono::hours(12);
constexpr size_t kMaxUploadBytes = 10 * 1024 * 1024;
constexpr char kSessionCookie[] = "phonebook_session";

class TemporaryFile {
public:
    explicit TemporaryFile(std::filesystem::path path) : path_(std::move(path)) {}
    ~TemporaryFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void sendJson(httplib::Response& response, int status, const Json& body) {
    response.status = status;
    response.set_content(body.dump(), "application/json; charset=utf-8");
}

int statusFor(const ServiceException& error) {
    switch (error.code()) {
    case ErrorCode::Unauthorized: return 401;
    case ErrorCode::Forbidden: return 403;
    case ErrorCode::NotFound: return 404;
    case ErrorCode::AlreadyExists:
    case ErrorCode::Duplicate: return 409;
    case ErrorCode::AccountLocked: return 403;
    case ErrorCode::Expired:
    case ErrorCode::InvalidInput: return 400;
    }
    return 400;
}

template <typename Operation>
void handleJson(httplib::Response& response, Operation&& operation) {
    try {
        response.status = 200;
        const Json result = operation();
        sendJson(response, response.status, result);
    } catch (const ServiceException& error) {
        sendJson(response, statusFor(error), {{"error", error.what()}});
    } catch (const nlohmann::json::exception& error) {
        sendJson(response, 400, {{"error", std::string("Dữ liệu không hợp lệ: ") + error.what()}});
    } catch (const std::invalid_argument& error) {
        sendJson(response, 400, {{"error", error.what()}});
    } catch (const std::exception& error) {
        std::cerr << "Request failed: " << error.what() << '\n';
        sendJson(response, 500, {{"error", "Đã xảy ra lỗi máy chủ."}});
    }
}

Json requestJson(const httplib::Request& request) {
    if (request.body.empty())
        throw std::invalid_argument("Vui lòng gửi dữ liệu JSON.");
    Json body = Json::parse(request.body);
    if (!body.is_object())
        throw std::invalid_argument("Dữ liệu phải là một đối tượng JSON.");
    return body;
}

std::string requiredString(const Json& body, const char* key) {
    const auto value = body.find(key);
    if (value == body.end() || !value->is_string())
        throw std::invalid_argument(std::string("Thiếu trường ") + key + ".");
    return value->get<std::string>();
}

std::string optionalString(const Json& body, const char* key) {
    const auto value = body.find(key);
    if (value == body.end() || value->is_null())
        return {};
    if (!value->is_string())
        throw std::invalid_argument(std::string("Trường ") + key + " phải là văn bản.");
    return value->get<std::string>();
}

std::string newSessionToken() {
    std::array<unsigned char, 32> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
        throw std::runtime_error("Không thể tạo mã phiên an toàn.");
    static constexpr char hex[] = "0123456789abcdef";
    std::string token;
    token.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) {
        token.push_back(hex[byte >> 4]);
        token.push_back(hex[byte & 0x0f]);
    }
    return token;
}

std::string sessionTokenFromCookie(const httplib::Request& request) {
    const std::string cookies = request.get_header_value("Cookie");
    size_t start = 0;
    while (start < cookies.size()) {
        const size_t end = cookies.find(';', start);
        const std::string item = cookies.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        const size_t equals = item.find('=');
        if (equals != std::string::npos &&
            item.substr(0, equals) == kSessionCookie) {
            return item.substr(equals + 1);
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
        while (start < cookies.size() && cookies[start] == ' ')
            ++start;
    }
    return {};
}

std::optional<RuntimeSession> authenticatedUser(
    const httplib::Request& request) {
    const std::string token = sessionTokenFromCookie(request);
    if (token.empty())
        return std::nullopt;
    std::lock_guard<std::mutex> lock(sessionsMutex);
    const auto found = sessions.find(token);
    if (found == sessions.end())
        return std::nullopt;
    if (found->second.expiresAt <= std::chrono::system_clock::now()) {
        sessions.erase(found);
        return std::nullopt;
    }
    return found->second.user;
}

template <typename Operation>
void handleAuthenticated(const httplib::Request& request,
                         httplib::Response& response,
                         Operation&& operation) {
    const auto user = authenticatedUser(request);
    if (!user) {
        sendJson(response, 401, {{"error", "Phiên đăng nhập đã hết hạn. Vui lòng đăng nhập lại."}});
        return;
    }
    handleJson(response, [&] { return operation(*user); });
}

int pathId(const httplib::Request& request) {
    if (request.matches.size() < 2)
        throw std::invalid_argument("Mã không hợp lệ.");
    size_t parsed = 0;
    const std::string value = request.matches[1].str();
    int id = 0;
    try {
        id = std::stoi(value, &parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument("Mã không hợp lệ.");
    }
    if (parsed != value.size() || id <= 0)
        throw std::invalid_argument("Mã không hợp lệ.");
    return id;
}

int queryInt(const httplib::Request& request, const char* key, int fallback = 0) {
    if (!request.has_param(key))
        return fallback;
    const std::string value = request.get_param_value(key);
    size_t parsed = 0;
    int result = 0;
    try {
        result = std::stoi(value, &parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string("Tham số ") + key + " không hợp lệ.");
    }
    if (parsed != value.size())
        throw std::invalid_argument(std::string("Tham số ") + key + " không hợp lệ.");
    return result;
}

Json contactJson(const Contact& contact) {
    const std::string phone = contact.phones.empty()
        ? std::string() : contact.phones.front().number;
    const std::string email = contact.emails.empty()
        ? std::string() : contact.emails.front().email;
    return {
        {"id", contact.idContact},
        {"lastName", contact.lastname},
        {"firstName", contact.firstname},
        {"fullName", contact.fullname},
        {"phone", phone},
        {"email", email},
        {"company", contact.company},
        {"address", contact.address},
        {"note", contact.note},
        {"favorite", contact.isFavorite},
        {"groupIds", contact.groupIds}
    };
}

RuntimeContact contactFromJson(const Json& body) {
    RuntimeContact contact;
    contact.lastName = requiredString(body, "lastName");
    contact.firstName = requiredString(body, "firstName");
    contact.phone = requiredString(body, "phone");
    contact.email = optionalString(body, "email");
    contact.company = optionalString(body, "company");
    contact.address = optionalString(body, "address");
    contact.note = optionalString(body, "note");
    return contact;
}

std::string csvFileName() {
    return newSessionToken() + ".csv";
}

void registerRoutes(httplib::Server& server) {
    server.set_payload_max_length(kMaxUploadBytes);

    server.Get("/api/health", [](const httplib::Request&, httplib::Response& response) {
        sendJson(response, 200, {{"status", "ok"}});
    });

    server.Post("/api/register", [](const httplib::Request& request,
                                    httplib::Response& response) {
        handleJson(response, [&] {
            const Json body = requestJson(request);
            std::lock_guard<std::mutex> lock(runtimeMutex);
            const int userId = runtime->registerUser(
                requiredString(body, "fullName"),
                requiredString(body, "username"),
                requiredString(body, "email"),
                requiredString(body, "password"));
            response.status = 201;
            return Json{{"userId", userId}};
        });
    });

    server.Post("/api/login", [](const httplib::Request& request,
                                  httplib::Response& response) {
        handleJson(response, [&] {
            const Json body = requestJson(request);
            RuntimeSession user;
            {
                std::lock_guard<std::mutex> lock(runtimeMutex);
                user = runtime->authenticate(
                    requiredString(body, "username"),
                    requiredString(body, "password"));
            }
            const std::string token = newSessionToken();
            {
                std::lock_guard<std::mutex> lock(sessionsMutex);
                sessions[token] = {user, std::chrono::system_clock::now() +
                                             kSessionLifetime};
            }
            response.set_header("Set-Cookie",
                std::string(kSessionCookie) + "=" + token +
                "; HttpOnly; SameSite=Strict; Path=/; Max-Age=43200");
            return Json{{"userId", user.userId}, {"name", user.name},
                        {"admin", user.admin}};
        });
    });

    server.Post("/api/logout", [](const httplib::Request& request,
                                   httplib::Response& response) {
        const std::string token = sessionTokenFromCookie(request);
        if (!token.empty()) {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            sessions.erase(token);
        }
        response.set_header("Set-Cookie",
            std::string(kSessionCookie) +
            "=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
        sendJson(response, 200, {{"ok", true}});
    });

    server.Get("/api/session", [](const httplib::Request& request,
                                   httplib::Response& response) {
        handleAuthenticated(request, response, [](const RuntimeSession& user) {
            return Json{{"userId", user.userId}, {"name", user.name},
                        {"admin", user.admin}};
        });
    });

    server.Post("/api/password/reset", [](const httplib::Request& request,
                                           httplib::Response& response) {
        handleJson(response, [&] {
            const Json body = requestJson(request);
            std::lock_guard<std::mutex> lock(runtimeMutex);
            runtime->resetPassword(requiredString(body, "token"),
                                   requiredString(body, "password"));
            return Json{{"ok", true}};
        });
    });

    server.Get("/api/contacts", [](const httplib::Request& request,
                                    httplib::Response& response) {
        handleAuthenticated(request, response, [&](const RuntimeSession& user) {
            const std::string keyword = request.has_param("q")
                ? request.get_param_value("q") : std::string();
            const int groupId = queryInt(request, "groupId");
            const bool favorites = request.has_param("favorites") &&
                request.get_param_value("favorites") == "true";
            const ContactSort sort = request.has_param("sort") &&
                request.get_param_value("sort") == "first"
                ? ContactSort::FirstNameAsc : ContactSort::LastNameAsc;
            std::vector<Contact> contacts;
            {
                std::lock_guard<std::mutex> lock(runtimeMutex);
                contacts = runtime->listContacts(
                    user.userId, keyword, groupId, favorites, sort);
            }
            Json result = Json::array();
            for (const Contact& contact : contacts)
                result.push_back(contactJson(contact));
            return result;
        });
    });

    server.Post("/api/contacts", [](const httplib::Request& request,
                                     httplib::Response& response) {
        handleAuthenticated(request, response, [&](const RuntimeSession& user) {
            const RuntimeContact contact = contactFromJson(requestJson(request));
            std::lock_guard<std::mutex> lock(runtimeMutex);
            const int id = runtime->addContact(user.userId, contact);
            response.status = 201;
            return Json{{"id", id}};
        });
    });

    server.Put(R"(/api/contacts/([0-9]+))",
        [](const httplib::Request& request, httplib::Response& response) {
            handleAuthenticated(request, response, [&](const RuntimeSession& user) {
                const int id = pathId(request);
                const RuntimeContact contact = contactFromJson(requestJson(request));
                std::lock_guard<std::mutex> lock(runtimeMutex);
                const auto contacts = runtime->listContacts(
                    user.userId, {}, 0, false, ContactSort::LastNameAsc);
                const auto existing = std::find_if(
                    contacts.begin(), contacts.end(),
                    [id](const Contact& item) { return item.idContact == id; });
                const bool wasFavorite =
                    existing != contacts.end() && existing->isFavorite;
                runtime->updateContact(user.userId, id, contact);
                if (wasFavorite)
                    runtime->toggleFavorite(user.userId, id);
                return Json{{"ok", true}};
            });
        });

    server.Delete(R"(/api/contacts/([0-9]+))",
        [](const httplib::Request& request, httplib::Response& response) {
            handleAuthenticated(request, response, [&](const RuntimeSession& user) {
                const int id = pathId(request);
                std::lock_guard<std::mutex> lock(runtimeMutex);
                runtime->removeContact(user.userId, id);
                return Json{{"ok", true}};
            });
        });

    server.Post(R"(/api/contacts/([0-9]+)/favorite)",
        [](const httplib::Request& request, httplib::Response& response) {
            handleAuthenticated(request, response, [&](const RuntimeSession& user) {
                const int id = pathId(request);
                std::lock_guard<std::mutex> lock(runtimeMutex);
                return Json{{"favorite", runtime->toggleFavorite(user.userId, id)}};
            });
        });

    server.Put(R"(/api/contacts/([0-9]+)/group)",
        [](const httplib::Request& request, httplib::Response& response) {
            handleAuthenticated(request, response, [&](const RuntimeSession& user) {
                const int id = pathId(request);
                const Json body = requestJson(request);
                const auto group = body.find("groupId");
                if (group == body.end() || !group->is_number_integer())
                    throw std::invalid_argument("Thiếu mã nhóm hợp lệ.");
                std::lock_guard<std::mutex> lock(runtimeMutex);
                runtime->setContactGroup(user.userId, id, group->get<int>());
                return Json{{"ok", true}};
            });
        });

    server.Get("/api/groups", [](const httplib::Request& request,
                                  httplib::Response& response) {
        handleAuthenticated(request, response, [](const RuntimeSession& user) {
            std::vector<RuntimeGroup> groups;
            {
                std::lock_guard<std::mutex> lock(runtimeMutex);
                groups = runtime->listGroups(user.userId);
            }
            Json result = Json::array();
            for (const RuntimeGroup& group : groups)
                result.push_back({{"id", group.id}, {"name", group.name}});
            return result;
        });
    });

    server.Post("/api/groups", [](const httplib::Request& request,
                                   httplib::Response& response) {
        handleAuthenticated(request, response, [&](const RuntimeSession& user) {
            const Json body = requestJson(request);
            std::lock_guard<std::mutex> lock(runtimeMutex);
            const int id = runtime->createGroup(
                user.userId, requiredString(body, "name"));
            response.status = 201;
            return Json{{"id", id}};
        });
    });

    server.Delete(R"(/api/groups/([0-9]+))",
        [](const httplib::Request& request, httplib::Response& response) {
            handleAuthenticated(request, response, [&](const RuntimeSession& user) {
                const int id = pathId(request);
                std::lock_guard<std::mutex> lock(runtimeMutex);
                runtime->removeGroup(user.userId, id);
                return Json{{"ok", true}};
            });
        });

    server.Post("/api/contacts/import", [](const httplib::Request& request,
                                            httplib::Response& response) {
        handleAuthenticated(request, response, [&](const RuntimeSession& user) {
            if (!request.has_file("file"))
                throw std::invalid_argument("Vui lòng chọn file CSV.");
            const auto uploaded = request.get_file_value("file");
            if (uploaded.content.size() > kMaxUploadBytes)
                throw std::invalid_argument("File CSV vượt quá giới hạn 10 MB.");
            TemporaryFile temp(std::filesystem::temp_directory_path() /
                               ("phonebook-import-" + csvFileName()));
            {
                std::ofstream output(temp.path(), std::ios::binary);
                if (!output)
                    throw std::runtime_error("Không thể tạo file tạm để nhập CSV.");
                output.write(uploaded.content.data(),
                             static_cast<std::streamsize>(uploaded.content.size()));
                if (!output)
                    throw std::runtime_error("Không thể ghi file CSV tạm.");
            }
            ImportResult result;
            {
                std::lock_guard<std::mutex> lock(runtimeMutex);
                result = runtime->importContacts(user.userId, temp.path().string());
            }
            return Json{{"imported", result.imported}, {"skipped", result.skipped},
                        {"errors", result.errors}};
        });
    });

    server.Get("/api/contacts/export", [](const httplib::Request& request,
                                           httplib::Response& response) {
        const auto user = authenticatedUser(request);
        if (!user) {
            sendJson(response, 401, {{"error", "Vui lòng đăng nhập lại."}});
            return;
        }
        try {
            TemporaryFile temp(std::filesystem::temp_directory_path() /
                               ("phonebook-export-" + csvFileName()));
            {
                std::lock_guard<std::mutex> lock(runtimeMutex);
                runtime->exportContacts(user->userId, temp.path().string());
            }
            std::ifstream input(temp.path(), std::ios::binary);
            std::ostringstream content;
            content << input.rdbuf();
            if (!input.good() && !input.eof())
                throw std::runtime_error("Không thể đọc file CSV đã xuất.");
            response.set_content(content.str(), "text/csv; charset=utf-8");
            response.set_header("Content-Disposition",
                                "attachment; filename=\"phonebook.csv\"");
        } catch (const ServiceException& error) {
            sendJson(response, statusFor(error), {{"error", error.what()}});
        } catch (const std::exception& error) {
            std::cerr << "CSV export failed: " << error.what() << '\n';
            sendJson(response, 500, {{"error", "Không thể xuất file CSV."}});
        }
    });

    server.Get("/api/admin/users", [](const httplib::Request& request,
                                       httplib::Response& response) {
        handleAuthenticated(request, response, [&](const RuntimeSession& user) {
            const std::string keyword = request.has_param("q")
                ? request.get_param_value("q") : std::string();
            std::vector<RuntimeUser> users;
            int count = 0;
            {
                std::lock_guard<std::mutex> lock(runtimeMutex);
                users = runtime->listUsers(user.userId, keyword);
                count = runtime->countUsers(user.userId);
            }
            Json result = Json::array();
            for (const RuntimeUser& account : users) {
                result.push_back({{"id", account.id}, {"name", account.name},
                                  {"active", account.active}});
            }
            return Json{{"users", result}, {"count", count}};
        });
    });

    server.Put(R"(/api/admin/users/([0-9]+)/active)",
        [](const httplib::Request& request, httplib::Response& response) {
            handleAuthenticated(request, response, [&](const RuntimeSession& user) {
                const int targetId = pathId(request);
                const Json body = requestJson(request);
                const auto active = body.find("active");
                if (active == body.end() || !active->is_boolean())
                    throw std::invalid_argument("Thiếu trạng thái tài khoản hợp lệ.");
                std::lock_guard<std::mutex> lock(runtimeMutex);
                runtime->setUserActive(user.userId, targetId, active->get<bool>());
                return Json{{"ok", true}};
            });
        });

    server.Post(R"(/api/admin/users/([0-9]+)/reset-token)",
        [](const httplib::Request& request, httplib::Response& response) {
            handleAuthenticated(request, response, [&](const RuntimeSession& user) {
                const int targetId = pathId(request);
                std::lock_guard<std::mutex> lock(runtimeMutex);
                return Json{{"token", runtime->issuePasswordResetToken(
                    user.userId, targetId)}};
            });
        });

    server.set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if (response.body.empty())
            sendJson(response, response.status, {{"error", "Không tìm thấy tài nguyên."}});
    });
}
}

int main() {
    const char* configuredDatabase = std::getenv("PHONEBOOK_DB_PATH");
    const std::filesystem::path databasePath = configuredDatabase && *configuredDatabase
        ? configuredDatabase : "data/phonebook.db";
    const std::filesystem::path webRoot = [] {
        const char* configured = std::getenv("PHONEBOOK_WEB_ROOT");
        return configured && *configured
            ? std::filesystem::path(configured)
            : std::filesystem::path(PHONEBOOK_WEB_ROOT);
    }();

    try {
        if (!std::filesystem::exists(webRoot / "index.html"))
            throw std::runtime_error("Không tìm thấy giao diện web tại " +
                                     webRoot.string());
        if (databasePath.has_parent_path())
            std::filesystem::create_directories(databasePath.parent_path());
        PhoneBookRuntime appRuntime(databasePath.string());
        runtime = &appRuntime;

        httplib::Server server;
        if (!server.set_mount_point("/", webRoot.string()))
            throw std::runtime_error("Không thể phục vụ giao diện web.");
        registerRoutes(server);

        const char* configuredPort = std::getenv("PORT");
        const int port = configuredPort && *configuredPort
            ? std::stoi(configuredPort) : 8080;
        if (port < 1 || port > 65535)
            throw std::runtime_error("PORT phải nằm trong khoảng 1 đến 65535.");
        std::cout << "PhoneBook web app listening on http://0.0.0.0:"
                  << port << " using database " << databasePath.string() << '\n';
        if (!server.listen("0.0.0.0", port))
            throw std::runtime_error("Không thể mở cổng HTTP.");
    } catch (const std::exception& error) {
        std::cerr << "Startup failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
