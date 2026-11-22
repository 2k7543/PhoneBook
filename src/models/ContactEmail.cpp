#include "models/ContactEmail.h"

#include "utils/Validator.h"

// Uses the shared email rule so forms, contacts, and CSV imports agree.
bool ContactEmail::isValid() const {
    return Validator::isValidEmail(email);
}

// Maps email labels between the model enum and their stored text values.
std::string toString(EmailLabel label) {
    switch (label) {
    case EmailLabel::Work: return "work";
    case EmailLabel::Other: return "other";
    case EmailLabel::Personal: return "personal";
    }
    return "personal";
}

EmailLabel emailLabelFromString(const std::string& value) {
    if (value == "work") return EmailLabel::Work;
    if (value == "other") return EmailLabel::Other;
    return EmailLabel::Personal;
}
