#ifndef _BASE64_H
#define _BASE64_H

#include <string>

bool is_base64(unsigned char c);
std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_decode(std::string const& s);

#endif