#include "service/CsvService.h"

#include "utils/Validator.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <sstream>

// Converts CSV rows into contacts and delegates validation/persistence to services.
CsvService::CsvService(ContactService& contactService, ContactRepository& contacts)
    : service_(contactService), contacts_(contacts) {}

// Quotes and escapes fields containing CSV delimiters or line breaks.
std::string CsvService::escape(const std::string& field) {
    if (field.find_first_of(",\"\n\r") == std::string::npos)
        return field;
    std::string escaped = "\"";
    for (char ch : field) {
        if (ch == '"')
            escaped += '"';
        escaped += ch;
    }
    return escaped + "\"";
}

// Parses one CSV record while honoring quoted commas and escaped quotes.
std::vector<std::string> CsvService::parseLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (ch == ',' && !quoted) {
            fields.push_back(Validator::trim(field));
            field.clear();
        } else {
            field += ch;
        }
    }
    fields.push_back(Validator::trim(field));
    return fields;
}

std::vector<std::string> CsvService::split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        item = Validator::trim(item);
        if (!item.empty())
            parts.push_back(item);
    }
    return parts;
}

// Exports contacts in batches using the current CSV column layout.
int CsvService::exportContacts(int userId, const std::string& filePath) const {
    std::ofstream output(filePath, std::ios::binary);
    if (!output)
        throw ServiceException(ErrorCode::InvalidInput,
                               "Không thể ghi file: " + filePath);

    output << "\xEF\xBB\xBF"
           << "lastname,firstname,fullname,phone,email,company,address,note\n";
    int exported = 0;
    constexpr int batchSize = 200;
    for (int offset = 0;; offset += batchSize) {
        const auto batch = contacts_.list(
            userId, 0, ContactSort::LastNameAsc, batchSize, offset);
        if (batch.empty())
            break;
        for (const Contact& contact : batch) {
            const std::string phone = contact.phones.empty()
                ? std::string() : contact.phones.front().number;
            const std::string email = contact.emails.empty()
                ? std::string() : contact.emails.front().email;
            const std::vector<std::string> fields = {
                contact.lastname, contact.firstname, contact.fullname,
                phone, email, contact.company, contact.address, contact.note
            };
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i)
                    output << ',';
                output << escape(fields[i]);
            }
            output << '\n';
            ++exported;
        }
    }
    if (!output)
        throw ServiceException(ErrorCode::InvalidInput, "Lỗi khi ghi file CSV.");
    return exported;
}

// Imports header-based or legacy rows and reports imported, skipped, and invalid rows.
ImportResult CsvService::importContacts(int userId, const std::string& filePath) {
    std::ifstream input(filePath, std::ios::binary);
    if (!input)
        throw ServiceException(ErrorCode::InvalidInput,
                               "Không thể đọc file: " + filePath);

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(std::move(line));
    }
    ImportResult result;
    if (lines.empty())
        return result;

    if (lines[0].compare(0, 3, "\xEF\xBB\xBF") == 0)
        lines[0].erase(0, 3);

    std::vector<std::string> header = parseLine(lines[0]);
    std::vector<std::string> normalized;
    normalized.reserve(header.size());
    for (std::string column : header) {
        column = Validator::toLower(Validator::trim(column));
        column.erase(std::remove_if(column.begin(), column.end(),
                                    [](unsigned char ch) {
                                        return std::isspace(ch) != 0;
                                    }),
                     column.end());
        normalized.push_back(std::move(column));
    }
    auto columnIndex = [&normalized](const std::string& key) {
        const auto found = std::find(normalized.begin(), normalized.end(), key);
        return found == normalized.end()
            ? -1 : static_cast<int>(std::distance(normalized.begin(), found));
    };
    auto columnIndexAny = [&columnIndex](
        std::initializer_list<std::string> keys) {
        for (const std::string& key : keys) {
            const int index = columnIndex(key);
            if (index >= 0)
                return index;
        }
        return -1;
    };
    // Accept both English and Vietnamese headers, plus the older positional format.
    const int fullnameIndex = columnIndexAny({"fullname", "họtên"});
    const int lastnameIndex = columnIndexAny({"lastname", "họ"});
    const int firstnameIndex = columnIndexAny({"firstname", "tên"});
    const int phoneIndex = columnIndexAny({"phone", "sốđiệnthoại"});
    const int emailIndex = columnIndex("email");
    const int companyIndex = columnIndexAny({"company", "côngty"});
    const int addressIndex = columnIndexAny({"address", "địachỉ"});
    const int noteIndex = columnIndexAny({"note", "ghichú"});
    const bool hasHeader = phoneIndex >= 0 &&
        (fullnameIndex >= 0 || lastnameIndex >= 0 || firstnameIndex >= 0);

    auto importRow = [&](const std::vector<std::string>& values, int lineNumber) {
        auto valueAt = [&values](int index) {
            return index >= 0 && index < static_cast<int>(values.size())
                ? values[static_cast<size_t>(index)] : std::string();
        };
        Contact contact;
        contact.lastname = hasHeader ? valueAt(lastnameIndex) : std::string();
        contact.firstname = hasHeader ? valueAt(firstnameIndex) : std::string();
        contact.fullname = hasHeader ? valueAt(fullnameIndex) : valueAt(0);
        if (contact.lastname.empty() || contact.firstname.empty()) {
            const auto parts = split(contact.fullname, ' ');
            if (!parts.empty()) {
                if (contact.lastname.empty())
                    contact.lastname = parts.front();
                if (contact.firstname.empty()) {
                    contact.firstname.clear();
                    for (size_t i = 1; i < parts.size(); ++i) {
                        if (!contact.firstname.empty())
                            contact.firstname += ' ';
                        contact.firstname += parts[i];
                    }
                }
            }
        }
        contact.fullname = Validator::trim(
            contact.lastname + " " + contact.firstname);

        const std::string phone = hasHeader ? valueAt(phoneIndex) : valueAt(1);
        const std::string email = hasHeader ? valueAt(emailIndex) : valueAt(2);
        contact.company = hasHeader ? valueAt(companyIndex) : valueAt(3);
        contact.address = hasHeader ? valueAt(addressIndex) : valueAt(4);
        contact.note = hasHeader ? valueAt(noteIndex) : valueAt(5);
        if (!phone.empty())
            contact.phones.push_back({0, phone, PhoneLabel::Mobile});
        if (!email.empty())
            contact.emails.push_back({0, email, EmailLabel::Personal});

        try {
            service_.add(userId, std::move(contact), false);
            ++result.imported;
        } catch (const ServiceException& error) {
            if (error.code() == ErrorCode::Duplicate)
                ++result.skipped;
            else
                result.errors.push_back(
                    "Dòng " + std::to_string(lineNumber) + ": " + error.what());
        }
    };

    for (size_t i = hasHeader ? 1 : 0; i < lines.size(); ++i) {
        if (Validator::trim(lines[i]).empty())
            continue;
        importRow(parseLine(lines[i]), static_cast<int>(i + 1));
    }
    return result;
}
