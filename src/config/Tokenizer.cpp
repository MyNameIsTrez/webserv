#include "Tokenizer.hpp"

#include "Config.hpp"

Tokenizer::Tokenizer(std::istream &file) : _file(file) {}

char Tokenizer::getWithoutWhitespace()
{
	char c;

	while (true)
	{
		_file.get(c);

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
		throw;
	}
	prevPos = _file.tellg();
	char c = getWithoutWhitespace();

	Token token;
	if (c == '"') // STRING
	{
		token.type = Token::STRING;
		token.string = "";
		_file.get(c);
		while (c != '"')
		{
			token.string += c;
			_file.get(c);
		}
	}
	else if (c == '{') // OBJECT_OPEN
	{
		token.type = Token::OBJECT_OPEN;
	}
	else if (c == '}') // OBJECT_CLOSE
	{
		token.type = Token::OBJECT_CLOSE;
	}
	else if (c == '[') // ARRAY_OPEN
	{
		token.type = Token::ARRAY_OPEN;
	}
	else if (c == ']') // ARRAY_CLOSE
	{
		token.type = Token::ARRAY_CLOSE;
	}
	else if (c == ',') // COMMA
	{
		token.type = Token::COMMA;
	}
	else if (c == ':') // COLON
	{
		token.type = Token::COLON;
	}
	else if (c >= '0' && c <= '9') // INTEGER
	{
		token.type = Token::INTEGER;
		token.string = c;
		while (c >= '0' && c <= '9')
		{
			std::streampos prevCharPos = _file.tellg();
			_file.get(c);

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
	else if (c == 't') // BOOLEAN_TRUE
	{
		if (!(get() == 'r' && get() == 'u' && get() == 'e'))
		{
			throw TokenExceptionExpectedBoolean();
		}
		token.type = Token::BOOLEAN_TRUE;
	}
	else if (c == 'f') // BOOLEAN_FALSE
	{
		if (!(get() == 'a' && get() == 'l' && get() == 's' && get() == 'e'))
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

bool Tokenizer::hasMoreTokens()
{
	return !_file.eof();
}
void Tokenizer::rollBackToken()
{
	if (_file.eof())
	{
		_file.clear();
	}
	_file.seekg(prevPos);
}
