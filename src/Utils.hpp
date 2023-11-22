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

std::string replaceAll(std::string input, const std::string &needle, const std::string &replacement);

template <class T> bool parseNumber(const std::string &str, T &number, T max);
} // namespace Utils

#include "Utils.tpp"
