#include "EmailSender.h"

#include <curl/curl.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
std::string environmentValue(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

bool hasLineBreak(const std::string& value) {
    return value.find_first_of("\r\n") != std::string::npos;
}

bool hasSmtpScheme(const std::string& url) {
    return url.compare(0, 7, "smtp://") == 0 ||
           url.compare(0, 8, "smtps://") == 0;
}

struct UploadBuffer {
    const std::string& content;
    std::size_t offset = 0;
};

std::size_t readContent(char* destination, std::size_t size,
                        std::size_t count, void* context) {
    auto& buffer = *static_cast<UploadBuffer*>(context);
    const std::size_t capacity = size * count;
    const std::size_t remaining = buffer.content.size() - buffer.offset;
    const std::size_t bytes = std::min(capacity, remaining);
    if (bytes > 0) {
        std::memcpy(destination, buffer.content.data() + buffer.offset, bytes);
        buffer.offset += bytes;
    }
    return bytes;
}

template <typename Value>
void setOption(CURL* handle, CURLoption option, Value value) {
    const CURLcode result = curl_easy_setopt(handle, option, value);
    if (result != CURLE_OK)
        throw std::runtime_error(curl_easy_strerror(result));
}

void initializeCurl() {
    static const CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK)
        throw std::runtime_error(curl_easy_strerror(result));
}
}

EmailSender::EmailSender()
    : smtpUrl_(environmentValue("PHONEBOOK_SMTP_URL")),
      username_(environmentValue("PHONEBOOK_SMTP_USERNAME")),
      password_(environmentValue("PHONEBOOK_SMTP_PASSWORD")),
      sender_(environmentValue("PHONEBOOK_SMTP_FROM")) {}

bool EmailSender::isConfigured() const {
    return hasSmtpScheme(smtpUrl_) && !username_.empty() &&
           !password_.empty() && !sender_.empty() &&
           !hasLineBreak(smtpUrl_) && !hasLineBreak(username_) &&
           !hasLineBreak(password_) && !hasLineBreak(sender_);
}

void EmailSender::sendPasswordResetCode(const std::string& recipient,
                                        const std::string& token) const {
    if (!isConfigured())
        throw std::runtime_error("SMTP settings are incomplete");
    if (recipient.empty() || token.empty() || hasLineBreak(recipient))
        throw std::runtime_error("Invalid password reset email data");

    initializeCurl();
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(
        curl_easy_init(), &curl_easy_cleanup);
    if (!handle)
        throw std::runtime_error("Could not initialize email transport");

    curl_slist* recipientList = curl_slist_append(nullptr, recipient.c_str());
    if (!recipientList)
        throw std::runtime_error("Could not create email recipient list");
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> recipients(
        recipientList, &curl_slist_free_all);

    const std::string message =
        "From: PhoneBook <" + sender_ + ">\r\n"
        "To: <" + recipient + ">\r\n"
        "Subject: PhoneBook password reset code\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n"
        "Ma dat lai mat khau PhoneBook cua ban la:\r\n\r\n" + token +
        "\r\n\r\nMa chi dung mot lan va het han sau 15 phut. "
        "Neu ban khong yeu cau dat lai mat khau, hay bo qua email nay.\r\n";
    UploadBuffer upload{message};

    setOption(handle.get(), CURLOPT_URL, smtpUrl_.c_str());
    setOption(handle.get(), CURLOPT_USERNAME, username_.c_str());
    setOption(handle.get(), CURLOPT_PASSWORD, password_.c_str());
    setOption(handle.get(), CURLOPT_USE_SSL,
              static_cast<long>(CURLUSESSL_ALL));
    setOption(handle.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    setOption(handle.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    setOption(handle.get(), CURLOPT_CONNECTTIMEOUT, 10L);
    setOption(handle.get(), CURLOPT_TIMEOUT, 30L);
    setOption(handle.get(), CURLOPT_NOSIGNAL, 1L);
    setOption(handle.get(), CURLOPT_MAIL_FROM, sender_.c_str());
    setOption(handle.get(), CURLOPT_MAIL_RCPT, recipients.get());
    setOption(handle.get(), CURLOPT_UPLOAD, 1L);
    setOption(handle.get(), CURLOPT_READFUNCTION, readContent);
    setOption(handle.get(), CURLOPT_READDATA, &upload);
    setOption(handle.get(), CURLOPT_INFILESIZE_LARGE,
              static_cast<curl_off_t>(message.size()));

    const CURLcode result = curl_easy_perform(handle.get());
    if (result != CURLE_OK)
        throw std::runtime_error(curl_easy_strerror(result));
}
