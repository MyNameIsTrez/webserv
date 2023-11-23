#include "Utils.hpp"

#include <algorithm>

bool Utils::startsWith(const std::string &haystack, const std::string &needle)
{
    return haystack.rfind(needle, 0) != std::string::npos;
}

std::string &Utils::ltrim(std::string &s, const char *t)
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

std::string &Utils::rtrim(std::string &s, const char *t)
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

std::string &Utils::trim(std::string &s, const char *t)
{
    return ltrim(rtrim(s, t), t);
}

std::string Utils::upper(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

std::string Utils::replaceAll(std::string input, const std::string &needle, const std::string &replacement)
{
    size_t start_pos = 0;
    while ((start_pos = input.find(needle, start_pos)) != std::string::npos)
    {
        input.replace(start_pos, needle.size(), replacement);
        start_pos += replacement.size(); // Handles case where 'replacement' is a substring of 'needle'
    }
    return input;
}
