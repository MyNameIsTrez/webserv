#pragma once

#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <string>

namespace Utils
{
	bool startsWith(const std::string &haystack, const std::string &needle);
	template <class T>
	bool parseNumber(const std::string &str, T &number, T max);

	struct SystemException : public std::runtime_error
	{
		SystemException(const std::string &function_name)
			: runtime_error("System exception in function '" + function_name + "': " + strerror(errno))
		{
		}
	};
	struct SystemExceptionGAI : public std::runtime_error
	{
		SystemExceptionGAI(const std::string &function_name, int errcode)
			: runtime_error("System exception in function '" + function_name + "': " + gai_strerror(errcode))
		{
		}
	};
}

#include "Utils.tpp"
