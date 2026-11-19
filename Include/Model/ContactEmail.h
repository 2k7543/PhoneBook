#pragma once
#include <string>
using namespace std;
enum EmailLabel
{
    PERSONAL, WORK, OTHER
};

class ContactEmail
{
    private:
    int idContactEmail;
    string email;

    public:
    ContactEmail();


    bool Vaild;

    string getEmail () const;
    EmailLabel getLabel() const;

    void setEmail(const string& email);
    void setLabel (EmailLabel label) ;
};