/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Parse.hpp                                          :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/28 17:04:16 by mforstho      #+#    #+#                 */
/*   Updated: 2023/08/30 17:50:37 by mforstho      ########   odam.nl         */
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

class Parse
{
private:
	std::vector<int> _ports;
	std::string _server_name;
	int _worker_connections;
	std::string _root_path;
	std::string _index_file;

public:
	typedef int (*t_jump_function)(std::string line);
	Parse(std::string file);
	virtual ~Parse(void);
	void save_config(std::string file);
	void save_type(std::string line, std::string type);
	std::string get_server_name(void);
	int get_connections(void);
	int get_port(size_t index);
	std::string get_root(void);
	std::string get_index_file(void);
	int call_conversion(std::string type, std::string line);
	void save_server_name(std::string line);
	void save_port(std::string line);
	void save_wc(std::string line);
	void save_root_path(std::string line);
	void save_index_file(std::string line);
};

#endif
