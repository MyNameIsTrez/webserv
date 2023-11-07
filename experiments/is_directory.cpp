#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

// c++ is_directory.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined -std=c++2b && ./a.out
int main(void)
{
	std::error_code ec;

	assert(std::filesystem::is_directory(".", ec)); assert(!ec);
	assert(std::filesystem::is_directory("../experiments", ec)); assert(!ec);

	assert(!std::filesystem::is_directory("a.out", ec)); assert(!ec);
	assert(!std::filesystem::is_directory("foo", ec)); assert(ec);

	return EXIT_SUCCESS;
}
