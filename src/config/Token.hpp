#pragma once

#include "Config.hpp"

struct Token
{
	enum
	{
		BOOLEAN_TRUE,
		BOOLEAN_FALSE,
		INTEGER,
		STRING,
		COMMA,
		COLON,
		ARRAY_OPEN,
		ARRAY_CLOSE,
		OBJECT_OPEN,
		OBJECT_CLOSE,
	} type;

	std::string string;
};