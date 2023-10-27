#pragma once

#include "Token.hpp"

class Tokenizer
{
public:
	Tokenizer(std::istream &file);

	char getWithoutWhitespace();
	Token getToken();
	bool hasMoreTokens();
	void rollBackToken();

private:
	char get()
	{
		char c;
		_file.get(c);
		return (c);
	}
	struct TokenException : public std::runtime_error
	{
		TokenException(const std::string &message) : std::runtime_error(message){};
	};
	struct TokenExceptionRanOutOfNonWhitespaceCharacters : public TokenException
	{
		TokenExceptionRanOutOfNonWhitespaceCharacters() : TokenException("Token exception: Ran out of non-whitespace characters"){};
	};
	struct TokenExceptionExpectedBoolean : public TokenException
	{
		TokenExceptionExpectedBoolean() : TokenException("Token exception: Expected boolean"){};
	};
	struct TokenExceptionNoMoreTokens : public TokenException
	{
		TokenExceptionNoMoreTokens() : TokenException("Token exception: No more tokens"){};
	};
	struct TokenExceptionUnexpectedCharacter : public TokenException
	{
		TokenExceptionUnexpectedCharacter() : TokenException("Token exception: Unexpected character"){};
	};

	std::istream &_file;
	size_t prevPos;
};
