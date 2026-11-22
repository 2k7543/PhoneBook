#include "utils/DateTime.h"

#include <ctime>
#include <iomanip>
#include <sstream>

// Formats local timestamps consistently across database records and the UI.
namespace {
std::string formatTime(std::time_t value, const char* format) {
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, format);
    return output.str();
}
}

// Provides current timestamps, expiry checks, and strict ISO date validation.
std::string DateTime::now() {
    return formatTime(std::time(nullptr), "%Y-%m-%d %H:%M:%S");
}

std::string DateTime::currentDate() {
    return formatTime(std::time(nullptr), "%Y-%m-%d");
}

std::string DateTime::afterMinutes(int minutes) {
    return formatTime(std::time(nullptr) + static_cast<std::time_t>(minutes) * 60,
                      "%Y-%m-%d %H:%M:%S");
}

bool DateTime::isExpired(const std::string& timestamp) {
    std::tm value{};
    std::istringstream input(timestamp);
    input >> std::get_time(&value, "%Y-%m-%d %H:%M:%S");
    if (input.fail())
        return true;
    const std::time_t expiry = std::mktime(&value);
    return expiry <= std::time(nullptr);
}

bool DateTime::isValidIsoDate(const std::string& date) {
    if (date.empty())
        return true;
    if (date.size() != 10 || date[4] != '-' || date[7] != '-')
        return false;
    std::tm parsed{};
    std::istringstream input(date);
    input >> std::get_time(&parsed, "%Y-%m-%d");
    if (input.fail())
        return false;
    std::ostringstream output;
    output << std::put_time(&parsed, "%Y-%m-%d");
    return output.str() == date;
}
