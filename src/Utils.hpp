#pragma once

#include <cstring>
#include <netdb.h>
#include <string>

namespace Utils
{
bool startsWith(const std::string &haystack, const std::string &needle);

std::string &ltrim(std::string &s, const char *t);
std::string &rtrim(std::string &s, const char *t);
std::string &trim(std::string &s, const char *t);

std::string upper(std::string str);

template <class T> bool parseNumber(const std::string &str, T &number, T max);
} // namespace Utils

#include "Utils.tpp"
