
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

std::vector<Token> tokenize_line(std::string line)
{
	std::vector<Token> tokens;
	std::string input;
	size_t i = 0;
	while (i < input.length())
	{
		if (input[i] == '=')
		{
			add_equals_token(input, i);
		}
		else if (is_whitespace(input))
		{
			add_whitespace_token(input, i);
		}
	}
}

void add_equals_token(size_t &i)
{
	Token token;
	token.str = "=";
	token.type == EQUALS;
	tokens.push_back(token);
	i++;
}

void add_whitespace_token(const std::string &input, size_t &i)
{
	i++;
}
