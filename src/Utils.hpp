#pragma once

#include <string>

namespace Utils
{
	bool startsWith(const std::string &haystack, const std::string &needle);
	template <class T>
	bool parseNumber(const std::string &str, T &number, T max);
}

#include "Utils.tpp"
