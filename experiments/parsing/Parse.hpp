/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Parse.hpp                                          :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/28 17:04:16 by mforstho      #+#    #+#                 */
/*   Updated: 2023/09/18 15:24:13 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#ifndef PARSE_HPP
#define PARSE_HPP

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <exception>
#include <algorithm>
#include "ServerData.hpp"

class Parse
{
private:
	// std::vector<int> _ports;
	// std::string _server_name;
	int _max_connections;
	// std::string _root_path;
	// std::string _index_file;
	std::vector<ServerData> _serverdata;

public:
	typedef int (*t_jump_function)(std::string line);
	Parse(std::string file);
	virtual ~Parse(void);
	int get_type(std::string type);
	void save_config(std::string file);
	void save_type(std::string line, std::string type);
	std::string get_server_name(void);
	int get_connections(void);
	int get_port(size_t index);
	std::string get_root(void);
	std::string get_index_file(void);
	// void save_server_name(std::string line);
	// void save_port(std::string line);
	void save_max_connections(std::string line);
	// void save_root_path(std::string line);
	// void save_index_file(std::string line);
	void new_server(std::string line, std::ifstream &config);
	void save_error_pages(std::string line, ServerData *new_server);
	void save_page(std::string line, ServerData *new_server);
	int check_line(std::string line);
	void print_server_info(size_t index);
};

#endif
