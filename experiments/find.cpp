#include <cassert>
#include <cstdlib>
#include <string>

// c++ find.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	std::string protocol;

	protocol = "HTTP/1.1";
	assert(protocol.find("HTTP/", 0) == 0);
	assert(protocol.rfind("HTTP/", 0) == 0);

	protocol = "aHTTP/1.1";
	assert(protocol.find("HTTP/", 0) == 1);
	assert(protocol.rfind("HTTP/", 0) == std::string::npos);

	return EXIT_SUCCESS;
}
