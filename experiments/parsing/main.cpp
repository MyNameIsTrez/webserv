/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   main.cpp                                           :+:    :+:            */
/*                                                     +:+                    */
/*   By: mforstho <mforstho@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2023/08/29 14:25:52 by mforstho      #+#    #+#                 */
/*   Updated: 2023/09/20 14:50:33 by mforstho      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "Parse.hpp"

int main(void)
{
	Parse parsed("config.txt");

	std::cout << std::endl;
	// std::cout << parsed.get_server_name() << std::endl;
	// std::cout << parsed.get_port(0) << std::endl;
	// std::cout << parsed.get_connections() << std::endl;
	// std::cout << parsed.get_root() << std::endl;
	// std::cout << parsed.get_index_file() << std::endl;

	parsed.print_server_info(0);
	// std::cout << std::endl;
	// parsed.print_server_info(1);

	return (EXIT_SUCCESS);
}
