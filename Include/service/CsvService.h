#pragma once
#include <string>
#include <vector>
#include "repositories/ContactRepository.h"
#include "service/ContactService.h"
using namespace std;

struct ImportResult {
    int imported = 0;
    int skipped = 0;
    vector<string> errors;
};

// Refactor note: CSV operations use the same validated contact service as
// manual entry, preventing imports from bypassing duplicate checks.
class CsvService {
public:
    CsvService(ContactService& contactService, ContactRepository& contacts);

    int exportContacts(int userId, const string& filePath) const;
    ImportResult importContacts(int userId, const string& filePath);

private:
    ContactService& service_;
    ContactRepository& contacts_;

    static string escape(const string& field);
    static vector < string > parseLine(const string& line);
    static vector < string > split(const string& s, char delim);
};
