#ifndef USERINFO_H
#define USERINFO_H

#include <string>

class userInfo {
public:
    bool new_user(std::string user, std::string pass);
    std::string findInfo(std::string username);
};

#endif