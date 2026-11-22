#include "models/Contact.h"

#include "utils/DateTime.h"
#include "utils/Validator.h"

// Validates the contact's required name and phone fields plus optional details.
bool Contact::validate(std::string* error) const {
    auto fail = [error](const std::string& message) {
        if (error)
            *error = message;
        return false;
    };
    if (!Validator::isValidFullName(fullname))
        return fail("Họ và tên không được để trống.");
    if (phones.empty())
        return fail("Vui lòng nhập số điện thoại.");
    for (const PhoneNumber& phone : phones) {
        if (!phone.isValid())
            return fail("Số điện thoại không hợp lệ.");
    }
    for (const ContactEmail& email : emails) {
        if (!email.email.empty() && !email.isValid())
            return fail("Địa chỉ email không hợp lệ.");
    }
    if (!birthday.empty() && !DateTime::isValidIsoDate(birthday))
        return fail("Ngày sinh phải theo định dạng YYYY-MM-DD.");
    if (error)
        error->clear();
    return true;
}
