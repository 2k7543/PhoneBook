#pragma once
using namespace std;
#include <string>

class Validator {
public:
    // Refactor note: shared normalization helpers support all service layers.
    static string trim(const string& value);
    static string toLower(const string& value);
    static bool isValidEmail(const string& email);
    static bool isValidPhoneNumber(const string& phone);
    static bool isValidUsername(const string& username);
    static bool isValidPassword(const string& password);
    static bool isValidFullName(const string& fullname);
    static bool isEmpty(const string& value);
};

