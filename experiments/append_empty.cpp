#include <cassert>
#include <string>

// c++ append_empty.cpp -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	std::string body("a");
	char received[] = "b";

	body.append(received, received + 1);
	assert(body == "ab");

	body.append(received + 1, received + 1);
	assert(body == "ab");

	return EXIT_SUCCESS;
}
