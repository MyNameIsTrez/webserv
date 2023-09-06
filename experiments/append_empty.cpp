#include <cassert>
#include <string>

// c++ append_empty.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	std::string body("");
	char received[] = "b";

	int a = 3;

	{
		int a = 5;
		(void)a;
	}

	(void)a;

	// body.append(received, received + 1);
	// assert(body == "ab");

	body.append(received + 1, received + 1);
	assert(body.empty());

	return EXIT_SUCCESS;
}
