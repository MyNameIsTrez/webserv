#pragma once
#ifndef TOKEN_HPP
#define TOKEN_HPP

#include "Config.hpp"

enum TokenType
{
	EQUALS,
	WORD,
	WHITESPACE,
	NEWLINE,
};

struct Token
{
	std::string str;
	TokenType type;
	size_t index;
};

std::vector<Token> tokenize_line(const std::string &input);

#endif
