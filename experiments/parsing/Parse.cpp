/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   parse.cpp                                          :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/29 11:52:13 by mforstho      #+#    #+#                 */
/*   Updated: 2023/08/30 17:49:10 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "Parse.hpp"

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
	index_file = 5
};

int get_type(std::string type)
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
}

int Parse::call_conversion(std::string type, std::string line)
{
	const static t_jump_function jump_table[] = {
		[server_name] = save_server_name,
	};
	return (jump_table[type](line));
}

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
		std::string type = line.substr(0, line.find('='));
		std::cout << type << std::endl;
		save_type(line, type);
	}
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
