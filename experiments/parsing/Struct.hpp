/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Struct.hpp                                         :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/28 17:04:16 by mforstho      #+#    #+#                 */
/*   Updated: 2023/08/28 17:41:21 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include <string>
#include <vector>

struct parsed
{
	std::vector<int> ports;
	std::string server_name;
	int worker_connections;
	std::string root_path;
	std::string index_file;
};
