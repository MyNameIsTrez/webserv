#include "Utils.hpp"

bool Utils::startsWith(const std::string &haystack, const std::string &needle)
{
	return haystack.rfind(needle, 0) == std::string::npos;
}
