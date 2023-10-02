/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Parse.cpp                                          :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/29 11:52:13 by mforstho      #+#    #+#                 */
/*   Updated: 2023/10/02 15:13:21 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "Parse.hpp"

#include <string> // temp
#include <map>

Parse::Parse(void)
{
}

Parse::Parse(std::string file)
{
	save_config(file);
}

Parse::~Parse(void)
{
}

enum parse_types
{
	invalid_type,
	server_name,
	listen,
	max_connections,
	root_path,
	index_file,
	new_server
};

int Parse::get_type(std::string type)
{
	if (type == "server_name")
		return (server_name);
	else if (type == "listen")
		return (listen);
	else if (type == "max_connections")
		return (max_connections);
	else if (type == "root_path")
		return (root_path);
	else if (type == "index_file")
		return (index_file);
	else
		return (invalid_type);
}

void Parse::save_type(std::string line, std::string type)
{
	if (type == "max_connections")
		return (save_max_connections(line));
	else if (type == "default_file")
		return (save_default_file(line));
	else
		throw InvalidLineException();
}

void Parse::save_max_connections(std::string line)
{
	_max_connections = std::stoi(line.substr(line.find('=') + 1));
}

void Parse::save_default_file(std::string line)
{
	_default_file = line.substr(line.find('=') + 1);
}

void Parse::save_config(std::string file)
{
	std::ifstream config(file);
	if (!config.is_open())
	{
		throw InvalidFileException();
	}
	std::string line;
	while (getline(config, line))
	{
		if (line.find_first_not_of('\t') == line.npos)
			continue;
		if (line.find("server {") != line.npos)
		{
			new_server(line, config);
			// std::cout << "			END OF SERVER" << std::endl;
		}
		else
		{
			if (line.find('=') == line.npos)
			{
				std::cout << "<" << line << ">" << std::endl;
				throw InvalidLineException();
			}
			line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
			std::string type = line.substr(0, line.find('='));
			std::cout << type << std::endl;
			if (type == "\n")
			{
				throw EmptyTypeException();
			}
			save_type(line, type);
		}
	}
}

void Parse::save_error_pages(std::string line, ServerData *new_server)
{
	int i = line.find("error_page");
	if (i != line.npos)
	{
		while (line[i] != '\0')
		{
			if (line[i] == '=')
				break;
			int page = 0;
			if (isdigit(line.c_str()[i]) != 0)
			{
				while (isdigit(line.c_str()[i]) != 0)
				{
					page *= 10;
					page += line.c_str()[i] - '0';
					i++;
				}
				new_server->_error_pages.insert(std::pair<int, std::string>(page, line.substr(line.find('=') + 2)));
			}
			i++;
		}
	}
}

void print_vector(std::string prefix, std::vector<std::string> nums) // temporary
{
	std::cout << prefix;
	for (int i = 0; i < nums.size(); i++)
	{
		std::cout << " " << nums.at(i);
	}
	std::cout << std::endl;
}

PageData Parse::save_page(std::string line, std::ifstream &config)
{
	// std::cout << "LOCATION BEGINNING" << std::endl;
	int page_start = line.find('/');
	if (page_start == line.npos)
	{
		throw InvalidLineException();
	}
	int page_end = line.find('{');
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
		{
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
				else
					new_page.autoindex = false; // TODO: wordt nu ook op false gezet bij gibberish
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
	return (new_page);
}

// "new_server" zou ik kunnen rewriten zodat het meteen in de "_serverdata" gezet wordt
// dan hoeven functies zoals save_page en save_error_pages geen "new_server" mee te krijgen
void Parse::new_server(std::string line, std::ifstream &config)
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
				save_page(line, config);
				unclosed--;
			}
			else if (line.find('=') != line.npos)
			{
				if (line.find("error_page") != line.npos) // is nog niet beschermd
					save_error_pages(line, &new_server);
				else
				{
					line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
					std::string type = line.substr(0, line.find('='));
					// std::cout << type << std::endl;
					std::string value = line.substr(line.find('=') + 1);
					// std::cout << "value of " << type << ": " << value << std::endl;
					if (value == "")
					{
						throw EmptyTypeException();
					}
					if (type == "server_name")
						new_server._server_name = value;
					else if (type == "listen")
						new_server._ports.push_back(std::stoi(value));
					else if (type == "root_path")
						new_server._root_path = value;
					else if (type == "index_file")
						new_server._index_file = value;
					else if (type == "client_max_body_size")
						new_server._client_max_body_size = std::stoi(value);
					else if (type == "http_redirection")
						new_server._http_redirection = value;
					else
					{
						std::cout << "Error on line:" << line << std::endl;
						throw InvalidLineException();
						exit(EXIT_FAILURE);
					}
				}
			}
		}
	}
	_serverdata.push_back(new_server);
	return;
}

void Parse::print_server_info(size_t index)
{
	std::cout << "Info for server " << index + 1 << std::endl;
	std::cout << "server name: " << _serverdata.at(index)._server_name << std::endl;
	std::cout << "server ports: ";
	for (int i = 0; i < _serverdata.at(index)._ports.size(); i++)
	{

		std::cout << _serverdata.at(index)._ports.at(i) << ", ";
	}
	std::cout << std::endl;
	std::cout << "root path: " << _serverdata.at(index)._root_path << std::endl;
	std::cout << "index file: " << _serverdata.at(index)._index_file << std::endl;
	for (std::map<int, std::string>::iterator it = _serverdata.at(index)._error_pages.begin(); it != _serverdata.at(index)._error_pages.end(); ++it)
	{
		std::cout << "error page: " << it->first << ": " << it->second << std::endl;
	}
}

/*
std::string new_server() {
	if ('{') {
		while (getline(config, line)) {

		}
		return line;
	}
}

line = new_server(config, line);

*/
