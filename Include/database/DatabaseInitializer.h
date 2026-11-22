#pragma once
#include <string>
#include "database/Database.h"
using namespace std;

class DatabaseInitializer {
public:
    // Refactor note: one schema initializer is used by startup and supports
    // migrating the existing Qt database in place.
    static void initialize(Database& db);

    static bool seedAdmin(Database& db, const string& email,
                          const string& fullname, const string& passwordHash);
};
