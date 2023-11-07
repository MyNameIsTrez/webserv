#include <cassert>
#include <cstdlib>
#include <string>

// c++ empty_substr.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	std::string foo;

	assert(foo.substr(0, 0) == "");

	return EXIT_SUCCESS;
}
