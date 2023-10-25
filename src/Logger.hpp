#pragma once

#include <string>
#include <iostream>

namespace Logger
{
	enum Level
	{
		DEBUG,
		INFO,
		WARNING,
		ERROR,
		NONE
	};

	extern Level level;

	template <typename T>
	void debug(const T &message)
	{
		if (level <= DEBUG)
			std::cout << "[DEBUG]: " << message << std::endl;
	}
	template <typename T>
	void info(const T &message)
	{
		if (level <= INFO)
			std::cout << "[INFO]: " << message << std::endl;
	}
	template <typename T>
	void warning(const T &message)
	{
		if (level <= WARNING)
			std::cout << "[WARNING]: " << message << std::endl;
	}
	template <typename T>
	void error(const T &message)
	{
		if (level <= ERROR)
			std::cout << "[ERROR]: " << message << std::endl;
	}
};
