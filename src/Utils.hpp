#pragma once
#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

template <typename T>
bool convert_digits(std::string input, T &dst)
{
	dst = 0;
	for (size_t i = 0; i < input.size(); i++)
	{
		if (input[i] < '0' || input[i] > '9')
			return (false);
		dst *= 10;
		dst += static_cast<T>(input[i] - '0');
	}
	return (true);
}

#endif
