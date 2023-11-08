#pragma once

#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <string>

namespace Utils
{
	bool startsWith(const std::string &haystack, const std::string &needle);

	std::string &ltrim(std::string &s, const char *t);
	std::string &rtrim(std::string &s, const char *t);
	std::string &trim(std::string &s, const char *t);

	template <class T>
	bool parseNumber(const std::string &str, T &number, T max);

	struct SystemException : public std::runtime_error
	{
		SystemException(const std::string &function_name)
			: runtime_error("System exception in function '" + function_name + "': " + strerror(errno))
		{
		}
		SystemException(const std::string &function_name, const std::string &error_message)
			: runtime_error("System exception in function '" + function_name + "': " + error_message)
		{
		}
	};
}

#include "Utils.tpp"
