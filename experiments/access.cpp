#include <cassert>
#include <cstdlib>
#include <string>
#include <unistd.h>

// c++ access.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
	assert(access(".", F_OK | X_OK) == 0);
	assert(access("a.out", F_OK | X_OK) == 0);
	assert(access("../experiments", F_OK | X_OK) == 0);

	assert(access("foo", F_OK | X_OK) != 0);

	return EXIT_SUCCESS;
}
