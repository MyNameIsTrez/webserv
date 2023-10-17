#include "Config.hpp"
#include "Token.hpp"
#include "Utils.hpp"

Config::Config(void) : max_connections(), default_file(), servers()
{
}

Config::~Config(void)
{
}

void Config::saveType(std::string type, std::string value)
{
	if (type == "max_connections")
		return saveMaxConnections(value);
	if (type == "default_file")
		return saveDefaultFile(value);
	throw InvalidLineException();
}

void Config::saveMaxConnections(std::string value)
{
	if (!Utils::parseNumber(value, max_connections))
		throw InvalidLineException();

	// max_connections = check_digit(value);
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
					std::cout << line << " THIS IS A COMMENT" << std::endl;
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
}

// TODO: Remove
static void print_vector(std::string prefix, std::vector<std::string> nums)
{
	std::cout << prefix;
	for (size_t i = 0; i < nums.size(); i++)
	{
		std::cout << " " << nums.at(i);
	}
	std::cout << std::endl;
}

PageData Config::savePage(std::string line, std::istream &config) // TODO: veranderen naar "save_location"
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
	PageData new_page;
	new_page.page_path = line.substr(page_start, page_end - page_start);
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
			std::cout << type << " = " << value << std::endl;
			if (type == "allow_methods")
			{
				std::stringstream ss(value);
				std::istream_iterator<std::string> begin(ss);
				std::istream_iterator<std::string> end;
				std::vector<std::string> vstrings(begin, end);
				new_page.allowed_methods = vstrings;
				print_vector("allow_methods =", new_page.allowed_methods);
			}
			else if (type == "autoindex")
			{
				if (value == "on")
					new_page.autoindex = true;
				else if (value == "off")
					new_page.autoindex = false;
				else
					throw InvalidLineException();
			}
			else if (type == "index_file")
			{
				new_page.index_file = value;
			}
			else if (type == "root_path")
			{
				new_page.root = value;
			}
			else if (type == "cgi_path")
			{
				std::stringstream ss(value);
				std::istream_iterator<std::string> begin(ss);
				std::istream_iterator<std::string> end;
				std::vector<std::string> vstrings(begin, end);
				new_page.cgi_paths = vstrings;
				// print_vector("cgi_paths =", new_page.cgi_paths);
			}
			else if (type == "cgi_ext")
			{
				std::stringstream ss(value);
				std::istream_iterator<std::string> begin(ss);
				std::istream_iterator<std::string> end;
				std::vector<std::string> vstrings(begin, end);
				new_page.cgi_ext = vstrings;
				// print_vector("cgi_extensions =", new_page.cgi_ext);
			}
			else
				throw InvalidLineException();
		}
	}
	// std::cout << "LOCATION: " << new_page.page_path << std::endl;
	return new_page;
}

// "new_server" zou ik kunnen rewriten zodat het meteen in de "serverdata" gezet wordt
// dan hoeven functies zoals savePage en save_error_pages geen "new_server" mee te krijgen
void Config::newServer(std::string line, std::istream &config)
{
	// std::cout << "			NEW_SERVER" << std::endl;
	ServerData new_server;

	int unclosed = 0;
	std::cout << line << std::endl;
	while (getline(config, line))
	{
		if (line[0] != '\0')
		{
			std::cout << line << std::endl;
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
				new_server.page_data.push_back(savePage(line, config));
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
						std::cout << line << " THIS IS A COMMENT" << std::endl;
						continue;
					}
					else if (t_line.at(ti).str == "error_page")
					{
						unsigned long e_code;
						ti++;
						if (ti < t_line.size() && t_line.at(ti).type == WHITESPACE)
							ti++;
						else
							throw InvalidLineException();
						if (ti < t_line.size() && t_line.at(ti).type == WORD)
						{
							// digit stuff
							if (!Utils::parseNumber(t_line.at(ti).str, e_code))
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
					unsigned long tmp_port;
					if (!Utils::parseNumber(value, tmp_port))
						throw InvalidLineException();
					new_server.ports.push_back(tmp_port);
				}
				else if (type == "root_path")
					new_server.root_path = value;
				else if (type == "index_file")
					new_server.index_file = value;
				else if (type == "client_max_body_size")
				{
					if (!Utils::parseNumber(value, new_server.client_max_body_size))
						throw InvalidLineException();
					// new_server.client_max_body_size = check_digit(value);
				}
				else if (type == "http_redirection")
					new_server.http_redirection = value;
				else
				{
					std::cout << "Error on line:" << line << std::endl;
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
	std::cout << "max_connections: " << max_connections << std::endl;
	std::cout << "default_file: " << default_file << std::endl;

	std::cout << std::endl;
	for (size_t i = 0; i < servers.size(); i++)
	{
		const ServerData &server = servers.at(i);

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
		std::cout << "root_path: " << server.root_path << std::endl;
		std::cout << "index_file: " << server.index_file << std::endl;
		std::cout << "client_max_body_size: " << server.client_max_body_size << std::endl;
		std::cout << "http_redirection: " << server.http_redirection << std::endl;
		for (auto it = server.error_pages.begin(); it != server.error_pages.end(); ++it)
		{
			std::cout << "error page: " << it->first << ": " << it->second << std::endl;
		}
		for (size_t h = 0; h < server.page_data.size(); h++)
		{
			std::cout << "page_path: " << server.page_data.at(h).page_path << std::endl;
			std::cout << "allowed_methods: ";
			for (size_t k = 0; k < server.page_data.at(h).allowed_methods.size(); k++)
			{
				std::cout << server.page_data.at(h).allowed_methods.at(k) << ", ";
			}
			std::cout << std::endl;
			std::cout << "autoindex: " << server.page_data.at(h).autoindex << std::endl;
			std::cout << "index_file: " << server.page_data.at(h).index_file << std::endl;
			std::cout << "root: " << server.page_data.at(h).root << std::endl;
			std::cout << "cgi_paths: ";
			for (size_t l = 0; l < server.page_data.at(h).cgi_paths.size(); l++)
			{
				std::cout << server.page_data.at(h).cgi_paths.at(l) << ", ";
			}
			std::cout << std::endl;
			std::cout << "cgi_extensions: ";
			for (size_t m = 0; m < server.page_data.at(h).cgi_ext.size(); m++)
			{
				std::cout << server.page_data.at(h).cgi_ext.at(m) << ", ";
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

		for (const std::string &port : server_it->ports)
		{
			// We don't care about the returned bool
			// that indicates whether the key was already present,
			// since this maps the first usage of the port
			port_to_default_server_index.emplace(
				port,
				server_index
			);

			uint16_t port_number;
			if (!Utils::parseNumber(port, port_number)) throw InvalidPortException();
			port_numbers.emplace(port_number);

			for (const std::string &server_name : server_it->server_names)
			{
				auto pair = http_host_header_to_server_index.emplace(
					server_name + ":" + port,
					server_index
				);

				// If the key was already present
				if (!pair.second)
				{
					throw ConflictingServerNameException();
				}
			}
		}
	}
}
