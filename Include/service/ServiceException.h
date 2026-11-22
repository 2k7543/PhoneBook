#pragma once
#include <stdexcept>
#include <string>
using namespace std;

enum class ErrorCode { InvalidInput, NotFound, AlreadyExists, Duplicate, Unauthorized, Forbidden, AccountLocked, Expired };

// Refactor note: typed service errors are the common boundary between
// repositories, business rules, and Qt error dialogs.
class ServiceException : public runtime_error {
public:
    ServiceException(ErrorCode code, const string& message) : runtime_error(message), code_(code) {}
    ErrorCode code() const { return code_; }
private:
    ErrorCode code_;
};
