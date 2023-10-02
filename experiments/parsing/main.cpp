/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   main.cpp                                           :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/29 14:25:52 by mforstho      #+#    #+#                 */
/*   Updated: 2023/10/02 13:47:59 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "Parse.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cout << "Need only one argument" << std::endl;
		return (EXIT_FAILURE);
	}
	Parse parsed;
	try
	{
		// parsed.save_config("config2.txt");
		parsed.save_config(argv[1]);
	}
	catch (InvalidLineException a)
	{
		std::cout << a.what() << std::endl;
		return (EXIT_FAILURE);
	}

	std::cout << std::endl;
	// std::cout << parsed.get_server_name() << std::endl;
	// std::cout << parsed.get_port(0) << std::endl;
	// std::cout << parsed.get_connections() << std::endl;
	// std::cout << parsed.get_root() << std::endl;
	// std::cout << parsed.get_index_file() << std::endl;

	// parsed.print_server_info(0);
	// std::cout << std::endl;
	// parsed.print_server_info(1);

	return (EXIT_SUCCESS);
}
