#pragma once

#include <string>

// Refactor note: this replaces the missing group model and provides one type
// for group repositories, services, and the Qt adapter.
struct ContactGroup {
    int groupId = 0;
    int userId = 0;
    std::string name;
};
