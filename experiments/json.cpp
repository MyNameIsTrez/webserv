#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>
#include <limits>

namespace Utils
{
	// bool startsWith(const std::string &haystack, const std::string &needle);
	template <class T>
	bool parseNumber(const std::string &str, T &number, T max);
}

template <class T>
bool Utils::parseNumber(const std::string &str, T &number, T max)
{
	number = 0;

	for (size_t i = 0; i < str.length(); i++)
	{
		char chr = str.at(i);
		if (chr < '0' || chr > '9')
			return false;

		char digit = chr - '0';

		if (number > max / 10)
			return false;
		number *= 10;

		if (number > max - digit)
			return false;
		number += digit;
	}

	return true;
}

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

struct Node
{
public:
	bool getBoolean() const
	{
		try
		{
			return std::get<bool>(_value);
		}
		catch (const std::bad_variant_access &a) // werkt gewoon
		{
			throw NODEExceptionGetBoolean();
		}
	}
	size_t getInteger() const
	{
		try
		{
			return std::get<size_t>(_value);
		}
		catch (const std::bad_variant_access &a) // werkt gewoon
		{
			throw NODEExceptionGetInteger();
		}
	}
	std::string getString() const
	{
		try
		{
			return std::get<std::string>(_value);
		}
		catch (const std::bad_variant_access &a) // werkt gewoon
		{
			throw NODEExceptionGetString();
		}
	}
	std::vector<Node> getArray() const
	{
		try
		{
			return std::get<std::vector<Node>>(_value);
		}
		catch (const std::bad_variant_access &a) // werkt gewoon
		{
			throw NODEExceptionGetArray();
		}
	}
	std::map<std::string, Node> getObject() const
	{
		try
		{
			return std::get<std::map<std::string, Node>>(_value);
		}
		catch (const std::bad_variant_access &a) // werkt gewoon
		{
			throw NODEExceptionGetObject();
		}
	}

	void setBoolean(bool boolean)
	{
		_value = boolean;
	}
	void setInteger(size_t integer)
	{
		_value = integer;
	}
	void setString(const std::string string)
	{
		_value = string;
	}
	void setArray(std::vector<Node> array)
	{
		_value = array;
	}
	void setObject(const std::map<std::string, Node> object)
	{
		_value = object;
	}

private:
	std::variant<bool, size_t, std::string, std::vector<Node>, std::map<std::string, Node>> _value;

	struct NodeException : public std::runtime_error
	{
		NodeException(const std::string &message) : std::runtime_error(message){};
	};
	struct NODEExceptionGetBoolean : public NodeException
	{
		NODEExceptionGetBoolean() : NodeException("Node exception: Attempt to get boolean failed"){};
	};
	struct NODEExceptionGetInteger : public NodeException
	{
		NODEExceptionGetInteger() : NodeException("Node exception: Attempt to get integer failed"){};
	};
	struct NODEExceptionGetString : public NodeException
	{
		NODEExceptionGetString() : NodeException("Node exception: Attempt to get string failed"){};
	};
	struct NODEExceptionGetArray : public NodeException
	{
		NODEExceptionGetArray() : NodeException("Node exception: Attempt to get array failed"){};
	};
	struct NODEExceptionGetObject : public NodeException
	{
		NODEExceptionGetObject() : NodeException("Node exception: Attempt to get object failed"){};
	};
};

class JSON
{
public:
	JSON(std::istream &file)
		: _tokenizer(file)
	{
		const Token token = _tokenizer.getToken();
		if (token.type != Token::OBJECT_OPEN)
			throw JSONExceptionExpectedObject();
		root = _parseObject();
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
	Node _parseInteger()
	{
		Node node;

		Token token = _tokenizer.getToken();
		size_t integer;
		if (!Utils::parseNumber(token.string, integer, std::numeric_limits<size_t>::max()))
			throw JSONExceptionInvalidInteger();
		node.setInteger(integer);

		return node;
	}
	Node _parseString()
	{
		Node node;

		Token token = _tokenizer.getToken();
		node.setString(token.string);

		return node;
	}
	Node _parseArray()
	{
		Node node;

		std::vector<Node> array;

		while (true)
		{
			Token valueToken = _tokenizer.getToken();
			if (valueToken.type == Token::ARRAY_CLOSE)
			{
				break;
			}
			switch (valueToken.type)
			{
			case Token::INTEGER:
			{
				_tokenizer.rollBackToken();
				array.push_back(_parseInteger());
				break;
			}
			case Token::STRING:
			{
				_tokenizer.rollBackToken();
				array.push_back(_parseString());
				break;
			}
			case Token::ARRAY_OPEN:
			{
				array.push_back(_parseArray());
				break;
			}
			case Token::OBJECT_OPEN:
			{
				array.push_back(_parseObject());
				break;
			}
			default:
			{
				throw JSONExceptionUnexpectedToken();
				break;
			}
			}
			Token token = _tokenizer.getToken();
			if (token.type == Token::ARRAY_CLOSE)
			{
				break;
			}
			if (token.type != Token::COMMA)
			{
				throw JSONExceptionExpectedComma();
			}
		}

		node.setArray(array);

		return node;
	}
	Node _parseObject()
	{
		Node node;

		std::map<std::string, Node> object;

		while (true)
		{
			Token keyToken = _tokenizer.getToken();
			if (keyToken.type == Token::OBJECT_CLOSE)
			{
				break;
			}
			if (keyToken.type != Token::STRING)
				throw JSONExceptionExpectedStringKey();
			const std::string &key = keyToken.string;

			if (_tokenizer.getToken().type != Token::COLON)
				throw JSONExceptionExpectedColon();

			Token valueToken = _tokenizer.getToken();

			switch (valueToken.type)
			{
			case Token::BOOLEAN_TRUE:
			case Token::BOOLEAN_FALSE:
			{
				_tokenizer.rollBackToken();
				std::string str;
				object.emplace(key, _parseBoolean());
				break;
			}
			case Token::INTEGER:
			{
				_tokenizer.rollBackToken();
				object.emplace(key, _parseInteger());
				break;
			}
			case Token::STRING:
			{
				_tokenizer.rollBackToken();
				object.emplace(key, _parseString());
				break;
			}
			case Token::ARRAY_OPEN:
			{
				object.emplace(key, _parseArray());
				break;
			}
			case Token::OBJECT_OPEN:
			{
				object.emplace(key, _parseObject());
				break;
			}
			default:
			{
				throw JSONExceptionUnexpectedToken();
				break;
			}
			}

			if (_tokenizer.getToken().type == Token::OBJECT_CLOSE)
			{
				break;
			}
		}

		node.setObject(object);

		return node;
	}

	struct JSONException : public std::runtime_error
	{
		JSONException(const std::string &message) : std::runtime_error(message){};
	};
	struct JSONExceptionExpectedStringKey : public JSONException
	{
		JSONExceptionExpectedStringKey() : JSONException("JSON exception: Expected string key"){};
	};
	struct JSONExceptionExpectedIntegerKey : public JSONException
	{
		JSONExceptionExpectedIntegerKey() : JSONException("JSON exception: Expected integer key"){};
	};
	struct JSONExceptionExpectedColon : public JSONException
	{
		JSONExceptionExpectedColon() : JSONException("JSON exception: Expected colon"){};
	};
	struct JSONExceptionExpectedComma : public JSONException
	{
		JSONExceptionExpectedComma() : JSONException("JSON exception: Expected comma"){};
	};
	struct JSONExceptionExpectedObject : public JSONException
	{
		JSONExceptionExpectedObject() : JSONException("JSON exception: Expected object"){};
	};
	struct JSONExceptionInvalidInteger : public JSONException
	{
		JSONExceptionInvalidInteger() : JSONException("JSON exception: Invalid integer"){};
	};
	struct JSONExceptionUnexpectedToken : public JSONException
	{
		JSONExceptionUnexpectedToken() : JSONException("JSON exception: Unexpected token"){};
	};

	Tokenizer _tokenizer;
};

struct ServerDirective
{
	std::vector<uint16_t> ports;
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

				if (key == "listen")
				{
					const Node &server_names_node = server_property_it.second;

					for (const auto &server_name_node : server_names_node.getArray())
					{
						const uint16_t &port = server_name_node.getInteger();

						server_directive.ports.push_back(port);
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
		ConfigException(const std::string &message) : std::runtime_error(message){};
	};
	struct ConfigExceptionUnknownKey : public ConfigException
	{
		ConfigExceptionUnknownKey() : ConfigException("Config exception: Unknown key"){};
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
			for (const uint16_t &port : server.ports)
			{
				std::cout << "port: '" << port << "'" << std::endl;
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

	// TODO:
	// if (!file.is_open()) throw ??;

	JSON json(file);

	Config config(json);

	Server server(config);
	server.run();
}
