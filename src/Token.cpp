#include "Token.hpp"

Token add_equals_token(size_t &i)
{
	Token token;
	token.str = "=";
	token.type = EQUALS;
	token.index = i;
	i++;
	return (token);
}

Token add_whitespace_token(const std::string &input, size_t &i)
{
	Token token;
	token.type = WHITESPACE;
	token.index = i;
	while (isspace(input[i]) != 0 && i < input.length())
		i++;
	token.str = input.substr(token.index, i - token.index);
	return (token);
}

Token add_word_token(const std::string &input, size_t &i)
{
	Token token;
	token.type = WORD;
	token.index = i;
	while (isspace(input[i]) == 0 && input[i] != '=' && input[i] != '\n' && i < input.length())
	{
		i++;
	}
	token.str = input.substr(token.index, i - token.index);
	return (token);
}

Token add_newline_token(size_t &i)
{
	Token token;
	token.type = NEWLINE;
	token.index = i;
	token.str = '\n';
	return (token);
}

std::vector<Token> tokenize_line(const std::string &input)
{
	std::cout << "tokenizing line: " << input << std::endl;
	std::vector<Token> tokenized_line;
	size_t i = 0;
	while (i < input.length())
	{
		if (input[i] == '=')
			tokenized_line.push_back(add_equals_token(i));
		else if (isspace(input[i]) != 0)
			tokenized_line.push_back(add_whitespace_token(input, i));
		else if (input[i] == '\n')
			tokenized_line.push_back(add_newline_token(i));
		else
			tokenized_line.push_back(add_word_token(input, i));
	}
	return (tokenized_line);
}
