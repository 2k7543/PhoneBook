#pragma once

#include <string>
#include <vector>

#include "models/ContactEmail.h"
#include "models/ContactPhoneNumber.h"

// Refactor note: one contact model now matches the active flat SQLite schema
// while retaining typed phone/email values for validation and CSV.
enum class ContactSort { LastNameAsc, FirstNameAsc, NameAsc = LastNameAsc };

struct Contact {
    int idContact = 0;
    int userId = 0;
    std::string fullname;
    std::string lastname;
    std::string firstname;
    std::string company;
    std::string birthday;
    std::string address;
    std::string note;
    std::string avatar;
    bool isFavorite = false;
    std::string createDate;
    std::string updateDate;
    std::vector<PhoneNumber> phones;
    std::vector<ContactEmail> emails;
    std::vector<int> groupIds;

    bool validate(std::string* error = nullptr) const;
};
