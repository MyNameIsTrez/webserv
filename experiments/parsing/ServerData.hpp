/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   ServerData.hpp                                     :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/31 11:54:00 by mforstho      #+#    #+#                 */
/*   Updated: 2023/09/20 16:01:53 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#ifndef SERVERDATA_HPP
#define SERVERDATA_HPP

#include <vector>
#include <map>

struct ServerData
{
	std::vector<int> _ports;
	std::string _server_name;
	std::string _root_path;
	std::string _index_file;
	int _client_max_body_size;
	std::string _http_redirection;
	std::map<int, std::string> error_pages;
};

struct PageData
{
	std::string page_path;
	std::vector<int> allowed_methods;
	bool autoindex;
	std::string index_file;
	std::string root;
	std::vector<std::string> cgi_paths;
	std::vector<std::string> cgi_ext;
};

#endif
