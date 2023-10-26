// clear && clang++ -fsanitize=undefined,address -fprofile-instr-generate -fcoverage-mapping --coverage foo.cpp && ./a.out
// See https://llvm.org/docs/LibFuzzer.html for more information

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	if (argc < 2)
		return (EXIT_SUCCESS);
	size_t length = strlen(argv[1]);
	if (length > 0 && argv[1][0] == 'H')
		if (length > 1 && argv[1][1] == 'I')
			if (length > 2 && argv[1][2] == '!')
				if (length > 14 && strcmp((char *)argv[1], "HI!abcdefghijkl") == 0)
					abort();

	return EXIT_SUCCESS;
}
