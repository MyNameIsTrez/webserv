#include "Utils.hpp"

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
