#pragma once

#include <string>

class EmailSender {
public:
    EmailSender();

    bool isConfigured() const;
    void sendPasswordResetCode(const std::string& recipient,
                               const std::string& token) const;

private:
    std::string smtpUrl_;
    std::string username_;
    std::string password_;
    std::string sender_;
};
