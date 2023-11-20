#include "Tokenizer.hpp"

#include "Config.hpp"

Tokenizer::Tokenizer(std::istream &file) : _file(file), _prevPos() {}

char Tokenizer::getWithoutWhitespace()
{
	char c;

	while (true)
	{
		c = _getChar();

		if (!isspace(c))
		{
			break;
		}
		else if (!_file.good())
		{
			if (isspace(c))
			{
				throw TokenExceptionRanOutOfNonWhitespaceCharacters();
			}
			break;
		}
	}

	return c;
}

Token Tokenizer::getToken()
{
	if (_file.eof())
	{
		throw TokenExceptionRanOutOfNonWhitespaceCharacters();
	}
	_prevPos = _file.tellg();
	char c = getWithoutWhitespace();

	Token token;
	if (c == '"')
	{
		token.type = Token::STRING;
		token.string = "";
		c = _getChar();

		while (c != '"')
		{
			token.string += c;
			c = _getChar();
		}
	}
	else if (c == '{')
	{
		token.type = Token::OBJECT_OPEN;
	}
	else if (c == '}')
	{
		token.type = Token::OBJECT_CLOSE;
	}
	else if (c == '[')
	{
		token.type = Token::ARRAY_OPEN;
	}
	else if (c == ']')
	{
		token.type = Token::ARRAY_CLOSE;
	}
	else if (c == ',')
	{
		token.type = Token::COMMA;
	}
	else if (c == ':')
	{
		token.type = Token::COLON;
	}
	else if (c >= '0' && c <= '9')
	{
		token.type = Token::INTEGER;
		token.string = c;
		while (c >= '0' && c <= '9')
		{
			std::streampos prevCharPos = _file.tellg();
			c = _getChar();

			if (_file.eof())
				break;
			else if (c >= '0' && c <= '9')
				token.string += c;
			else
			{
				_file.seekg(prevCharPos);
				break;
			}
		}
	}
	else if (c == 't')
	{
		if (!(_getChar() == 'r' && _getChar() == 'u' && _getChar() == 'e'))
		{
			throw TokenExceptionExpectedBoolean();
		}
		token.type = Token::BOOLEAN_TRUE;
	}
	else if (c == 'f')
	{
		if (!(_getChar() == 'a' && _getChar() == 'l' && _getChar() == 's' && _getChar() == 'e'))
		{
			throw TokenExceptionExpectedBoolean();
		}
		token.type = Token::BOOLEAN_FALSE;
	}
	else
	{
		throw TokenExceptionUnexpectedCharacter();
	}

	return token;
}

void Tokenizer::rollBackToken()
{
	if (_file.eof())
	{
		_file.clear();
	}
	_file.seekg(_prevPos);
}

char Tokenizer::_getChar()
{
	char c = _file.get();
	if (_file.eof()) throw TokenExceptionRanOutOfNonWhitespaceCharacters();
	return c;
}
