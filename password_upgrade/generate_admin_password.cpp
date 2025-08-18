#include <iostream>
#include <string>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <random>

using namespace std;

string generate_salt(int length = 16) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    
    string salt;
    for (int i = 0; i < length; i++) {
        salt += charset[dis(gen)];
    }
    return salt;
}

string hash_password(const string& password, const string& salt) {
    string salted_password = salt + password;
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)salted_password.c_str(), salted_password.length(), hash);
    
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    
    return salt + "$" + ss.str();
}

int main() {
    string password = "admin123";
    string hashed = hash_password(password, generate_salt());
    
    cout << "原密码: " << password << endl;
    cout << "加密后: " << hashed << endl;
    cout << endl;
    cout << "SQL更新语句:" << endl;
    cout << "UPDATE user SET passwd = '" << hashed << "' WHERE username = 'admin';" << endl;
    
    return 0;
}