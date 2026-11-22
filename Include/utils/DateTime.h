#pragma once
using namespace std;
#include <string>

class DateTime {
public:
    // Refactor note: date helpers complete the APIs required by reset-token
    // expiry, contact validation, and audit records.
    static string now();
    static string currentDate();
    static string afterMinutes(int minutes);
    static bool isExpired(const string& timestamp);
    static bool isValidIsoDate(const string& date);
};


