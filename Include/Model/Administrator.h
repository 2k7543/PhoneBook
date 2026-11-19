#pragma once
#include <User.h>
#include <Vector>

class Administrator : public user
{
    public:
    Administrator();

    vector <user> listUser(const string& keyword, int page);

    bool lockUser(int userId);
    bool unlockUser(int userId);
    bool resetUserPassword(int userId);
    bool deleteUser(int userId);
    void viewStatistics();
};