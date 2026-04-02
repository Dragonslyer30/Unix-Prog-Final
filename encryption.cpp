#include <iostream>
#include <random>
#include <unordered_map>
#include "encryption.h"
using namespace std;

    unordered_map<char, char> e;
    unordered_map<char, char> d;
    string key = "75/kBIwdq,8<_h)'c>0;R[CrsG\"z.T:b93#!QL~P2-4iKMH{|NXZn\%lOmJ1x^V(}Wvjg=oapFuef@U\\YAtS+E&`]$6yD*?";

    encryption::encryption() {
       for (char c = 33; c < 127; c++) {
            e[c] = key[(int)(c-33)];
            d[key[(int)(c-33)]] = c;
        }
    }

    string encryption::encrypt(const string s) {
        string result;
        for (char c : s) {
            if (e.count(c))
                result += e[c];
            else
                result += c; // leave unchanged
        }
        return result;
    }

    string encryption::decrypt(const string s) {
        string result;
        for (char c : s) {
            if (d.count(c))
                result += d[c];
            else
                result += c;
        }
        return result;
    }


