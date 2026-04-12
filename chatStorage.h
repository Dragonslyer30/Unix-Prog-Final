#ifndef CHATSTORAGE_H
#define CHATSTORAGE_H

#include <string>

using namespace std;


class chatStorage {
public:
    void storeChat(string chat, string text);
    string getChat(string chat);
};

#endif