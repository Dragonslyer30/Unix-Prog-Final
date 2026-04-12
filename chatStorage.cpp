#include <fstream>
#include "encryption.h"
#include <iostream>
#include "chatStorage.h"

using namespace std;


encryption en;

void chatStorage::storeChat(string chat, string text){
    string fileName = "chatText/" + chat + ".txt";
    ofstream file(fileName, ios::app);
    if (file.is_open()){
        file << en.encrypt(text);
        file.close();
    }
}

string chatStorage::getChat(string chat){
    string fileName = "chatText/" + chat + ".txt";
    string line;
    string chatText;

    ifstream file(fileName);

    if (file.is_open()){
        while (getline(file, line)){
            chatText += en.decrypt(line) + "\n";
        }
    }

    return chatText;
}
