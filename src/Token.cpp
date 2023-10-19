#include "Token.hpp"

Token add_equals_token(size_t &i)
{
	Token token;
	token.str = "=";
	token.type = EQUALS;
	i++;
	return (token);
}

Token add_whitespace_token(const std::string &input, size_t &i)
{
	Token token;
	token.type = WHITESPACE;
	size_t index = i;
	while (isspace(input[i]) && i < input.length())
		i++;
	token.str = input.substr(index, i - index);
	return (token);
}

// "[index][ ][=][foo][ ][\n]"
// "[index][ ][=][foo bar][\n]"
// "[index][ ][=][foo bar][ ]"
Token add_word_token(const std::string &input, size_t &i)
{
	Token token;
	token.type = WORD;
	size_t sentence_start_index = i;

	while (true)
	{
		// TODO: Remove
		// std::cerr << "input: '" << input << "', i: " << i << std::endl;

		// Include word
		while (i < input.length() && !isspace(input[i]) && input[i] != '=')
			i++;

		// Lookahead for a new word
		size_t lookahead_index = i;
		while (lookahead_index < input.length() && isspace(input[lookahead_index]) && input[lookahead_index] != '\n')
			lookahead_index++;

		if (lookahead_index == input.length() || isspace(input[lookahead_index]) || input[lookahead_index] == '=')
			break;
		else
			i = lookahead_index;
	}

	token.str = input.substr(sentence_start_index, i - sentence_start_index);
	return (token);
}

// Token add_newline_token(size_t &i)
// {
// 	Token token;
// 	token.type = NEWLINE;
// 	size_t index = i;
// 	token.str = '\n';
// 	return (token);
// }

std::vector<Token> tokenize_line(const std::string &input)
{
	// std::cout << "tokenizing line: " << input << std::endl;
	std::vector<Token> tokenized_line;
	size_t i = 0;
	while (i < input.length())
	{
		if (input[i] == '=')
			tokenized_line.push_back(add_equals_token(i));
		else if (isspace(input[i]))
			tokenized_line.push_back(add_whitespace_token(input, i));
		// else if (input[i] == '\n')
		// 	tokenized_line.push_back(add_newline_token(i));
		else
			tokenized_line.push_back(add_word_token(input, i));
	}
	return (tokenized_line);
}
