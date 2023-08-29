/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   parse.cpp                                          :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/29 11:52:13 by mforstho      #+#    #+#                 */
/*   Updated: 2023/08/29 16:11:02 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "Parse.hpp"

Parse::Parse(std::string file) {
	save_config(file);
}

Parse::~Parse(void) {
}

void Parse::save_type(std::string line, std::string type)
{
	if (type == "server_name")
	{
		_server_name = line.substr(line.find('=') + 1);
	}
	else if (type == "listen")
	{
		_ports.push_back(std::stof(line.substr(line.find('=') + 1)));
	}
	else if (type == "worker_connections")
	{
		_worker_connections = std::stoi(line.substr(line.find('=') + 1));
	}
	else if (type == "root_path")
	{
		_root_path = line.substr(line.find('=') + 1);
	}
	else if (type == "index_file")
	{
		_index_file = line.substr(line.find('=') + 1);
	}
	else {
		throw std::exception;
	}
	return;
}

void Parse::save_config(std::string file) {
	std::ifstream config(file);
	if (!config.is_open())
	{
		throw std::exception::file_no_access;
	}
	std::string line;
	while (getline(config, line))
	{
		std::string type = line.substr(0, line.find('='));
		std::cout << type << std::endl;
		save_type(line, type);
	}
}

std::string Parse::get_server_name(void) {
	return (_server_name);
}

int	Parse::get_connections(void) {
	return(_worker_connections);
}

int Parse::get_port(size_t index) {
	return (_ports.at(index));
}

std::string Parse::get_root(void) {
	return (_root_path);
}

std::string Parse::get_index_file(void) {
	return (_index_file);
}
