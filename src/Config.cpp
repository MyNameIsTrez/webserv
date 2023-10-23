#include "Config.hpp"

#include "Token.hpp"
#include "Utils.hpp"

#include <limits>

Config::Config(void)
	: connection_queue_length()
	, default_file()
	, servers()
	, port_to_server_index()
{
}

Config::~Config(void)
{
}

void Config::saveType(std::string type, std::string value)
{
	if (type == "connection_queue_length")
		return saveConnectionQueueLength(value);
	if (type == "default_file")
		return saveDefaultFile(value);
	throw InvalidLineException();
}

void Config::saveConnectionQueueLength(std::string value)
{
	if (!Utils::parseNumber(value, connection_queue_length, std::numeric_limits<int>::max()))
		throw InvalidLineException();
}

void Config::saveDefaultFile(std::string value)
{
	default_file = value;
}

void Config::init(std::istream &config)
{
	std::string line;
	while (getline(config, line))
	{
		if (line.find_first_not_of('\t') == line.npos)
			continue;
		if (line.find("server {") != line.npos)
		{
			newServer(line, config);
			// std::cout << "			END OF SERVER" << std::endl;
		}
		else
		{
			std::string type;
			std::string value;
			size_t ti = 0; // token_index
			std::vector<Token> t_line = tokenize_line(line);
			if (t_line.at(ti).type == WHITESPACE)
				ti++;
			if (ti < t_line.size() && t_line.at(ti).type == WORD)
			{
				if (t_line.at(ti).str.c_str()[0] == '#')
				{
					// std::cout << line << " THIS IS A COMMENT" << std::endl;
					continue;
				}
				else
					type = t_line.at(ti).str;
				ti++;
			}
			else
				throw InvalidLineException();
			if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
				ti++;
			if (ti < t_line.size() && t_line.at(ti).type == EQUALS)
				ti++;
			else
				throw InvalidLineException();
			if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
				ti++;
			if (ti < t_line.size() && t_line.at(ti).type == WORD)
				value = t_line.at(ti).str;
			else
				throw InvalidLineException();

			// if (line.find('=') == line.npos)
			// {
			// 	std::cout << "<" << line << ">" << std::endl;
			// 	throw InvalidLineException();
			// }
			// line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
			// std::string type = line.substr(0, line.find('='));
			// std::cout << type << std::endl;
			// if (type == "\n")
			// {
			// 	throw EmptyTypeException();
			// }
			saveType(type, value);
		}
	}

	initMetadata();
}

// TODO: Remove
// static void print_vector(std::string prefix, std::vector<std::string> nums)
// {
// 	std::cout << prefix;
// 	for (size_t i = 0; i < nums.size(); i++)
// 	{
// 		std::cout << " " << nums.at(i);
// 	}
// 	std::cout << std::endl;
// }

LocationDirective Config::saveLocation(std::string line, std::istream &config)
{
	// std::cout << "LOCATION BEGINNING" << std::endl;
	size_t page_start = line.find('/');
	if (page_start == line.npos)
	{
		throw InvalidLineException();
	}
	size_t page_end = line.find('{');
	if (page_end == line.npos)
	{
		throw InvalidLineException();
	}
	LocationDirective new_page{};
	new_page.uri = line.substr(page_start, page_end - page_start);
	while (getline(config, line))
	{
		if (line.find('}') != line.npos)
			break;
		if (line[0] != '\0')
		{ // TODO: hier ook tokenization gebruiken
			if (line.find('=') == line.npos)
				throw InvalidLineException();
			std::string value = line.substr(line.find('=') + 1);
			line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
			std::string type = line.substr(0, line.find('='));
			// std::cout << type << " = " << value << std::endl;
			if (type == "get_allowed")
			{
				new_page.get_allowed = parseBool(value);
			}
			else if (type == "post_allowed")
			{
				new_page.post_allowed = parseBool(value);
			}
			else if (type == "delete_allowed")
			{
				new_page.delete_allowed = parseBool(value);
			}
			else if (type == "autoindex")
			{
				new_page.autoindex = parseBool(value);
			}
			else if (type == "index")
			{
				new_page.index = value;
			}
			else if (type == "root")
			{
				new_page.root = value;
			}
			else if (type == "cgi_path")
			{
				// std::stringstream ss(value);
				// std::istream_iterator<std::string> begin(ss);
				// std::istream_iterator<std::string> end;
				// std::vector<std::string> vstrings(begin, end);
				// new_page.cgi_paths = vstrings;
				// print_vector("cgi_paths =", new_page.cgi_paths);
				new_page.cgi_paths.push_back(value);
			}
			else if (type == "cgi_ext")
			{
				// std::stringstream ss(value);
				// std::istream_iterator<std::string> begin(ss);
				// std::istream_iterator<std::string> end;
				// std::vector<std::string> vstrings(begin, end);
				// new_page.cgi_ext = vstrings;
				// print_vector("cgi_extensions =", new_page.cgi_ext);
				new_page.cgi_ext.push_back(value);
			}
			else
				throw InvalidLineException();
		}
	}
	// std::cout << "Location URI: " << new_page.uri << std::endl;
	return new_page;
}

// "new_server" zou ik kunnen rewriten zodat het meteen in de "serverdata" gezet wordt
// dan hoeven functies zoals saveLocation en saveErrorPages geen "new_server" mee te krijgen
void Config::newServer(std::string line, std::istream &config)
{
	// std::cout << "			NEW_SERVER" << std::endl;
	ServerDirective new_server{};

	int unclosed = 0;
	// std::cout << line << std::endl;
	while (getline(config, line))
	{
		if (line[0] != '\0')
		{
			// std::cout << line << std::endl;
			if (line.find('}') != line.npos)
			{
				if (unclosed == 0)
					break;
				else
					unclosed--;
			}
			else if (line.find('{') != line.npos)
				unclosed++;
			if (line.find("location") != line.npos)
			{
				new_server.locations.push_back(saveLocation(line, config));
				unclosed--;
			}
			else if (line.find('=') != line.npos)
			{
				std::string type;
				std::string value;
				size_t ti = 0; // token_index
				std::vector<Token> t_line = tokenize_line(line);
				if (t_line.at(ti).type == WHITESPACE)
					ti++;
				if (ti < t_line.size() && t_line.at(ti).type == WORD)
				{
					if (t_line.at(ti).str.c_str()[0] == '#')
					{
						// std::cout << line << " THIS IS A COMMENT" << std::endl;
						continue;
					}
					else if (t_line.at(ti).str == "error_page")
					{
						int e_code;
						ti++;
						if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
							ti++;
						else
							throw InvalidLineException();
						if (ti < t_line.size() && t_line.at(ti).type == WORD)
						{
							if (!Utils::parseNumber(t_line.at(ti).str, e_code, std::numeric_limits<int>::max()))
								throw InvalidLineException();
							ti++;
						}
						else
							throw InvalidLineException();
						if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
							ti++;
						if (ti < t_line.size() && t_line.at(ti).type == EQUALS)
							ti++;
						else
							throw InvalidLineException();
						if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
							ti++;
						if (ti < t_line.size() && t_line.at(ti).type == WORD)
						{
							value = t_line.at(ti).str;
							new_server.error_pages.emplace(static_cast<Status::Status>(e_code), value);
						}
						else
							throw InvalidLineException();
						continue;
					}
					else
						type = t_line.at(ti).str;
					ti++;
				}
				else
					throw InvalidLineException();
				if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
					ti++;
				if (ti < t_line.size() && t_line.at(ti).type == EQUALS)
					ti++;
				else
					throw InvalidLineException();
				if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
					ti++;
				if (ti < t_line.size() && t_line.at(ti).type == WORD)
					value = t_line.at(ti).str;
				else
					throw InvalidLineException();
				// line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
				// std::string type = line.substr(0, line.find('='));
				// std::cout << type << std::endl;
				// std::string value = line.substr(line.find('=') + 1);
				// std::cout << "value of " << type << ": " << value << std::endl;
				if (value == "")
				{
					throw EmptyTypeException();
				}
				if (type == "server_name")
					new_server.server_names.push_back(value);
				else if (type == "listen")
				{
					uint16_t port;
					if (!Utils::parseNumber(value, port, static_cast<uint16_t>(65535)))
						throw InvalidPortException();
					new_server.ports.push_back(port);
				}
				else if (type == "client_max_body_size")
				{
					if (!Utils::parseNumber(value, new_server.client_max_body_size, std::numeric_limits<size_t>::max()))
						throw InvalidLineException();
				}
				else if (type == "http_redirection")
					new_server.http_redirection = value;
				else
				{
					std::cerr << "Error on line:" << line << std::endl;
					throw InvalidLineException();
					exit(EXIT_FAILURE);
				}
			}
		}
	}
	servers.push_back(new_server);
	return;
}

// TODO: REMOVE?
void Config::printConfigInfo(void)
{
	std::cout << std::endl;
	std::cout << std::endl;
	std::cout << "connection_queue_length: " << connection_queue_length << std::endl;
	std::cout << "default_file: " << default_file << std::endl;

	std::cout << std::endl;
	for (size_t i = 0; i < servers.size(); i++)
	{
		const ServerDirective &server = servers.at(i);

		for (const std::string &server_name : server.server_names)
		{
			std::cout << "server_name: " << server_name << std::endl;
		}
		std::cout << "server ports: ";
		for (size_t j = 0; j < server.ports.size(); j++)
		{
			std::cout << server.ports.at(j) << ", ";
		}
		std::cout << std::endl;
		std::cout << "client_max_body_size: " << server.client_max_body_size << std::endl;
		std::cout << "http_redirection: " << server.http_redirection << std::endl;
		for (auto it = server.error_pages.begin(); it != server.error_pages.end(); ++it)
		{
			std::cout << "error page: " << it->first << ": " << it->second << std::endl;
		}
		for (size_t h = 0; h < server.locations.size(); h++)
		{
			const LocationDirective &location = server.locations.at(h);

			std::cout << "uri: " << location.uri << std::endl;
			std::cout << "get_allowed: " << location.get_allowed << std::endl;
			std::cout << "post_allowed: " << location.post_allowed << std::endl;
			std::cout << "delete_allowed: " << location.delete_allowed << std::endl;
			std::cout << std::endl;
			std::cout << "autoindex: " << location.autoindex << std::endl;
			std::cout << "index: " << location.index << std::endl;
			std::cout << "root: " << location.root << std::endl;
			std::cout << "cgi_paths: ";
			for (size_t l = 0; l < location.cgi_paths.size(); l++)
			{
				std::cout << location.cgi_paths.at(l) << ", ";
			}
			std::cout << std::endl;
			std::cout << "cgi_extensions: ";
			for (size_t m = 0; m < location.cgi_ext.size(); m++)
			{
				std::cout << location.cgi_ext.at(m) << ", ";
			}
			std::cout << std::endl;
			std::cout << std::endl;
		}
	}
}

void Config::initMetadata(void)
{
	for (auto server_it = servers.begin(); server_it < servers.end(); server_it++)
	{
		size_t server_index = server_it - servers.begin();

		for (const uint16_t &port : server_it->ports)
		{
			auto pair = port_to_server_index.emplace(
				port,
				server_index
			);

			// If the key was already present
			if (!pair.second)
			{
				throw DuplicatePortException();
			}
		}
	}
}

bool Config::parseBool(const std::string &value)
{
	if (value == "true")
		return true;
	else if (value == "false")
		return false;
	throw InvalidLineException();
}
