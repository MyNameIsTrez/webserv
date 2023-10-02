/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Parse.hpp                                          :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/28 17:04:16 by mforstho      #+#    #+#                 */
/*   Updated: 2023/10/02 14:08:44 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#ifndef PARSE_HPP
#define PARSE_HPP

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <exception>
#include <algorithm>
#include "ServerData.hpp"

class Parse
{
private:
public:
	int _max_connections;
	std::string _default_file;
	std::vector<ServerData> _serverdata;
	typedef int (*t_jump_function)(std::string line);
	Parse(void);
	Parse(std::string file);
	virtual ~Parse(void);
	int get_type(std::string type);
	void save_config(std::string file);
	void save_type(std::string line, std::string type);
	int get_port(size_t index);
	std::string get_root(void);
	std::string get_index_file(void);
	void save_max_connections(std::string line);
	void save_default_file(std::string line);
	void new_server(std::string line, std::ifstream &config);
	void save_error_pages(std::string line, ServerData *new_server);
	PageData save_page(std::string line, std::ifstream &config);
	int check_line(std::string line);
	void print_server_info(size_t index);
};

class InvalidLineException : public std::exception
{
public:
	std::string what()
	{
		return ("Error: Invalid line in config file");
	}
};

class InvalidFileException : public std::exception
{
public:
	std::string what()
	{
		return ("Error: Unable to open config file");
	}
};

class EmptyTypeException : public std::exception
{
public:
	std::string what()
	{
		return ("Error: Empty type in config file");
	}
};

#endif
