#pragma once
#include <string>
using namespace std;

enum UserRole
{
    User,
    Administrator
};

class user
{
    private:
        int idUser;  
        string FullName;
        string Email;
        string PasswordHash;
        UserRole Role;
        bool Acctive;
        string language;
        string CreateDate;
    public:
        user();
//get
        int GetIdUser() const;
        string getFullName() const;
        string getEmail() const;
        string getAcctive() const;
        string getLanguage() const;

//set
        void setFullName(const string& name);
        void setLanguage(const string& lang);

        bool  Register() ;
        bool login(const string& email,
                    const string& password);
        bool resetPassword(const string& email);
        bool updateProfile(const string& fullname, 
                const string& password, 
                const string& email);
        bool logOut();
};


