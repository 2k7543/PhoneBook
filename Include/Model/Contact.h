#pragma once
#include <string>
#include <vector>
#include <ContactPhoneNumber.h>
#include <ContactEmail.h>


class Contact
{
    private:
        int idContact;
        string fullName;
        string company;
        string birthday;
        string address;
        string note;
        string avatar;
        bool Favorite;
        string createDate;
        string updateDate;

        vector <ContactPhone> Phonebooks;
        vector <ContactEmail> Emails;



    public:
    Contact();

    bool add;
    bool update;
    bool delete;
    bool Favorite;
    bool validate;


//get
    int getIdContact() const;
    string getFullname() const;
    bool getFavorite() const;

//
   

// 

    void setFullname(const string& fullname) ;
    void setCompany(const string& company);
    void setAddress(const string& address);
    void setNote(const string& note);
    void setFavorite(bool status);


    void addPhoneNumber (const ContactPhone& phone);
    void addEmail (const ContactEmail& email);

    const vector <ContactPhone>& getPhoneNumber() const;
    const vector <ContactEmail>& getEmail() const;
    
};