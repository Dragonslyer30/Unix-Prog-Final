#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <string>

class encryption {
public:
    std::string encrypt(const std::string s);
    std::string decrypt(const std::string s);
    encryption();
};

#endif