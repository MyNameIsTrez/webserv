#include <dirent.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

// c++ readdir.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	DIR *dirp = opendir(".");
	if (dirp == NULL)
		return EXIT_FAILURE;

	dirent *dp;
	while ((dp = readdir(dirp)) != NULL)
	{
		std::cout << dp->d_name << std::endl;
	}

	closedir(dirp);

	return EXIT_SUCCESS;
}
