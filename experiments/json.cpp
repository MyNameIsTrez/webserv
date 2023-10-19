#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

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

class Tokenizer
{
public:
	Tokenizer(std::istream &file)
		: _file(file)
	{
	}

    char getWithoutWhitespace()
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
    Token getToken()
	{
		Token token;

		// TODO: Tokenize
		token.type = Token::BOOLEAN_TRUE;

		return token;
	}
    bool hasMoreTokens()
    {
        return !_file.eof();
    }
    void rollBackToken()
	{
        if (_file.eof())
        {
            _file.clear();
        }
        _file.seekg(prevPos);
	}

private:
	struct TokenException : public std::runtime_error
	{
		TokenException(const std::string &message) : std::runtime_error(message) {};
	};
	struct TokenExceptionRanOutOfNonWhitespaceCharacters : public TokenException
	{
		TokenExceptionRanOutOfNonWhitespaceCharacters() : TokenException("Token exception: Ran out of non-whitespace characters") {};
	};

	std::istream &_file;
	size_t prevPos;
};

struct Node
{
public:
	bool getBoolean() const
	{
		return std::get<bool>(_value);
	}
	int getInteger() const
	{
		return std::get<int>(_value);
	}
	std::string getString() const
	{
		return std::get<std::string>(_value);
	}
	std::vector<Node> getArray() const
	{
		return std::get<std::vector<Node>>(_value);
	}
	std::map<std::string, Node> getObject() const
	{
		return std::get<std::map<std::string, Node>>(_value);
	}

	void setBoolean(bool boolean)
	{
		_value = boolean;
	}
	void setInteger();
	void setString(const std::string string)
	{
		_value = string;
	}
	void setArray();
	void setObject(const std::map<std::string, Node> object)
	{
		_value = object;
	}

private:
	std::variant<bool, int, std::string, std::vector<Node>, std::map<std::string, Node>> _value;
};

class JSON
{
public:
	JSON(std::istream &file)
		: _root_initialized(false)
		, _tokenizer(file)
	{
		while (_tokenizer.hasMoreTokens())
		{
			const Token token = _tokenizer.getToken();
			switch (token.type)
			{
				case Token::BOOLEAN_TRUE:
				{
					break;
				}
				case Token::BOOLEAN_FALSE:
				{
					break;
				}
				case Token::INTEGER:
				{
					break;
				}
				case Token::STRING:
				{
					_tokenizer.rollBackToken();
					Node parsedString = _parseString();
					if (!_root_initialized)
					{
						root = parsedString;
						_root_initialized = true;
					}
					break;
				}
				case Token::COMMA:
				{
					break;
				}
				case Token::COLON:
				{
					break;
				}
				case Token::ARRAY_OPEN:
				{
					break;
				}
				case Token::ARRAY_CLOSE:
				{
					break;
				}
				case Token::OBJECT_OPEN:
				{
					Node parsedObject = _parseObject();
					if (!_root_initialized)
					{
						root = parsedObject;
						_root_initialized = true;
					}
					break;
				}
				case Token::OBJECT_CLOSE:
				{
					break;
				}
			}
		}
	}

	Node root;

private:
	Node _parseBoolean()
	{
		Node node;

		Token token = _tokenizer.getToken();
		node.setBoolean(token.type == Token::BOOLEAN_TRUE);

		return node;
	}
	Node _parseInteger();
	Node _parseString()
	{
		Node node;

		Token token = _tokenizer.getToken();
		node.setString(token.string);

		return node;
	}
	Node _parseArray();
	Node _parseObject()
	{
		Node node;

		std::map<std::string, Node> object;

		while (true)
		{
			if (!_tokenizer.hasMoreTokens())
				throw JSONExceptionNoMoreTokens();

			Token keyToken = _tokenizer.getToken();
			if (keyToken.type != Token::STRING)
				throw JSONExceptionExpectedStringKey();
			const std::string &key = keyToken.string;

			if (_tokenizer.getToken().type != Token::COLON)
				throw JSONExceptionExpectedColon();

			Token valueToken = _tokenizer.getToken();
			// const std::string &value = valueToken.string;

			switch (valueToken.type)
			{
				case Token::BOOLEAN_TRUE:
				{
					break;
				}
				case Token::BOOLEAN_FALSE:
				{
					break;
				}
				case Token::INTEGER:
				{
					break;
				}
				case Token::STRING:
				{
					_tokenizer.rollBackToken();
					object.emplace(key, _parseString());
					break;
				}
				case Token::COMMA:
				{
					break;
				}
				case Token::COLON:
				{
					break;
				}
				case Token::ARRAY_OPEN:
				{
					break;
				}
				case Token::ARRAY_CLOSE:
				{
					break;
				}
				case Token::OBJECT_OPEN:
				{
					break;
				}
				case Token::OBJECT_CLOSE:
				{
					break;
				}
			}

			if (_tokenizer.getToken().type == Token::OBJECT_CLOSE) {
				break;
			}
		}

		node.setObject(object);

		return node;
	}

	struct JSONException : public std::runtime_error
	{
		JSONException(const std::string &message) : std::runtime_error(message) {};
	};
	struct JSONExceptionNoMoreTokens : public JSONException
	{
		JSONExceptionNoMoreTokens() : JSONException("JSON exception: No more tokens") {};
	};
	struct JSONExceptionExpectedStringKey : public JSONException
	{
		JSONExceptionExpectedStringKey() : JSONException("JSON exception: Expected string key") {};
	};
	struct JSONExceptionExpectedColon : public JSONException
	{
		JSONExceptionExpectedColon() : JSONException("JSON exception: Expected colon") {};
	};

	bool _root_initialized;
	Tokenizer _tokenizer;
};


struct ServerDirective
{
	std::vector<std::string> server_names;
};

class Config
{
public:
	Config(const JSON &json)
	{
		for (const auto &server : json.root.getObject().at("servers").getArray())
		{
			ServerDirective server_directive;

			for (const auto &server_property_it : server.getObject())
			{
				const std::string &key = server_property_it.first;

				if (key == "server_names")
				{
					const Node &server_names_node = server_property_it.second;

					for (const auto &server_name_node : server_names_node.getArray())
					{
						const std::string &server_name = server_name_node.getString();

						server_directive.server_names.push_back(server_name);
					}
				}
				else
				{
					throw ConfigExceptionUnknownKey();
				}
			}

			servers.push_back(server_directive);
		}
	}

	std::vector<ServerDirective> servers;

private:
	struct ConfigException : public std::runtime_error
	{
		ConfigException(const std::string &message) : std::runtime_error(message) {};
	};
	struct ConfigExceptionUnknownKey : public ConfigException
	{
		ConfigExceptionUnknownKey() : ConfigException("Config exception: Unknown key") {};
	};
};

class Server
{
public:
	Server(const Config &config) : _config(config) {}

	void run()
	{
		for (const auto &server : _config.servers)
		{
			for (const std::string &server_name : server.server_names)
			{
				std::cout << "server_name: '" << server_name << "'" << std::endl;
			}
		}
	}

private:
	const Config &_config;
};

// clear && c++ json.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wswitch -Wimplicit-fallthrough -Wfatal-errors -g -fsanitize=address,undefined -std=c++2b && ./a.out
int main()
{
	std::ifstream file("webserv.json");

	JSON json(file);

	Config config(json);

	Server server(config);
	server.run();
}
