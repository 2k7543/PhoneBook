#pragma once
#include <string>

using namespace std;
enum PhoneLabel
{
    MOBILE,
    HOME,
    WORD,
    OTHER
};


class ContactPhone
{
    private:
    int idContactPhone;
    string number;
    PhoneLabel label;


    public:
    ContactPhone();


    bool vaid()const;
    string normallze()const;
    bool getNumber()const;
    PhoneLabel label() const;


    void setNumber(const string& number);
    void GetPhoneLabel(PhoneLabel label);
};