#pragma once

#include "Utils.hpp"

template <class T>
bool Utils::parseNumber(const std::string &str, T &number, T max)
{
	number = 0;

	// Don't accept leading zeros
	if (str.length() > 0 && str.at(0) == '0')
		return false;

	for (size_t i = 0; i < str.length(); i++)
	{
		char chr = str.at(i);
		if (chr < '0' || chr > '9')
			return false;

		T digit = T(chr) - '0';

		if (number > max / 10)
			return false;
		number *= 10;

		if (number > max - digit)
			return false;
		number += digit;
	}

	return true;
}
