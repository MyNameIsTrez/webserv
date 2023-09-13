/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Parse.cpp                                          :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/29 11:52:13 by mforstho      #+#    #+#                 */
/*   Updated: 2023/09/13 13:10:38 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "Parse.hpp"

#include <string> // temp
#include <map>

Parse::Parse(std::string file)
{
	save_config(file);
}

Parse::~Parse(void)
{
}

enum parse_types
{
	invalid_type = 0,
	server_name = 1,
	listen = 2,
	worker_connections = 3,
	root_path = 4,
	index_file = 5,
	new_server = 6
};

int Parse::get_type(std::string type)
{
	if (type == "server_name")
		return (server_name);
	else if (type == "listen")
		return (listen);
	else if (type == "worker_connections")
		return (worker_connections);
	else if (type == "root_path")
		return (root_path);
	else if (type == "index_file")
		return (index_file);
	else
		return (invalid_type);
}

void Parse::save_type(std::string line, std::string type)
{
	if (type == "server_name")
		return (save_server_name(line));
	else if (type == "listen")
		return (save_port(line));
	else if (type == "worker_connections")
		return (save_wc(line));
	else if (type == "root_path")
		return (save_root_path(line));
	else if (type == "index_file")
		return (save_index_file(line));
	else
		return;
}

// int Parse::call_conversion(std::string type, std::string line)
// {
// 	const static t_jump_function jump_table[] = {
// 		[server_name] = save_server_name,
// 	};
// 	return (jump_table[get_type(type)](line));
// }

void Parse::save_server_name(std::string line)
{
	_server_name = line.substr(line.find('=') + 1);
}

void Parse::save_port(std::string line)
{
	_ports.push_back(std::stof(line.substr(line.find('=') + 1)));
}

void Parse::save_wc(std::string line)
{
	_worker_connections = std::stoi(line.substr(line.find('=') + 1));
}

void Parse::save_root_path(std::string line)
{
	_root_path = line.substr(line.find('=') + 1);
}

void Parse::save_index_file(std::string line)
{
	_index_file = line.substr(line.find('=') + 1);
}

std::string Parse::get_server_name(void)
{
	return (_server_name);
}

int Parse::get_connections(void)
{
	return (_worker_connections);
}

int Parse::get_port(size_t index)
{
	return (_ports.at(index));
}

std::string Parse::get_root(void)
{
	return (_root_path);
}

std::string Parse::get_index_file(void)
{
	return (_index_file);
}

// int Parse::check_line(std::string line)
// {
// 	int type;
// 	if (line.find('='))
// 	{

// 		type = get_type(line.substr(0, line.find('=')));
// 		if (type == invalid_type)
// 		{
// 			// throw error;
// 		}
// 		if (line.c_str()[line.find('=') + 1] == '\n')
// 		{
// 			// throw error: field empty
// 		}
// 		int equalscounter = 0;
// 		for (int i = 0; line.c_str()[i] != '\0'; i++)
// 		{
// 			if (line.c_str()[i] == '=')
// 				equalscounter++;
// 			if (equalscounter > 1)
// 			{
// 				// throw error: too many '=' in line
// 			}
// 		}
// 	}
// 	else
// 	{
// 		// throw error;
// 	}
// 	std::cout << type << std::endl;
// 	// save_type(line, type);
// 	return (EXIT_SUCCESS);
// }

void Parse::save_config(std::string file)
{
	std::ifstream config(file);
	if (!config.is_open())
	{
		// throw std::exception::file_no_access;
	}
	std::string line;
	while (getline(config, line))
	{
		if (line.find("server {") != line.npos)
		{
			new_server(line, config);
			std::cout << "			END OF SERVER" << std::endl;
		}
		else
		{
			line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
			std::string type = line.substr(0, line.find('='));
			std::cout << type << std::endl;
			save_type(line, type);
		}
	}
}

void save_error_pages(std::string line)
{
	int i = line.find("error_page");
	if (i != line.npos)
	{
		while (line[i] != '\0')
		{
			int page = 0;
			while (isdigit(line.c_str()[i]) != 0)
			{
				std::cout << "ep test" << std::endl;
				page *= 10;
				page += line.c_str()[i] - '0';
				i++;
				std::cout << "error_page: " << page << std::endl;
			}
			if (line[i] == '=')
			{
				// save error page and link it to the given error codes
				break;
			}
			i++;
		}
	}
}

void Parse::new_server(std::string line, std::ifstream &config)
{
	std::cout << "			NEW_SERVER" << std::endl;
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
			if (line.find('=') != line.npos)
			{
				line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
				std::string type = line.substr(0, line.find('='));
				// std::cout << type << std::endl;
				std::string value = line.substr(line.find('=') + 1);
				// std::cout << "value of " << type << ": " << value << std::endl;
				if (value == "")
				{
					std::cout << "Error: empty value" << std::endl;
					return;
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
				// else
				// {
				// 	std::cout << "Error: invalid setting in config file" << std::endl;
				// 	return;
				// }
				// save_type(line, type);
			}
			save_error_pages(line);
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
