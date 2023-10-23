#pragma once

#include "Config.hpp"

enum TokenType
{
	EQUALS,
	WORD,
	WHITESPACE,
	// NEWLINE, // TODO: Remove?
};

struct Token
{
	std::string str;
	TokenType type;
};

std::vector<Token> tokenize_line(const std::string &input);
