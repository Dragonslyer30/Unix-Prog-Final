#include <iostream>
#include <unordered_map>
#include <fstream>
#include "userInfo.h"
#include "encryption.h"
using namespace std;

bool userInfo::new_user(string user, string pass) {
    encryption en;
    ifstream read("usernames_pass.txt");
    string existingUser, existingPass;

    // Check for duplicate usernames
    if (read.is_open()) {
        while (read >> existingUser >> existingPass) {
            if (en.decrypt(existingUser) == user) {
                read.close();
                return false;
            }
        }
        read.close();
    }

    // Now append new user
    ofstream file("usernames_pass.txt", ios::app);
    if (file.is_open()) {
        file << en.encrypt(user) << " " << en.encrypt(pass) << endl;
        file.close();
        return true;
    }

    cout << "Error opening file\n";
    return false;
}


string userInfo::findInfo(string username) {
    encryption en;
    ifstream file("usernames_pass.txt");
    string user, pass;

    if (file.is_open()) {
        while (file >> user >> pass) {
            if (en.decrypt(user) == username) {
                file.close();
                return en.decrypt(pass);  // ✅ FIX
            }
        }
        file.close();
    }

    return "null";
}