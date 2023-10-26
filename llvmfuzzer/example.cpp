// clear && clang++ -fsanitize=fuzzer,undefined,address -fprofile-instr-generate -fcoverage-mapping example.cpp && ./a.out
// See https://llvm.org/docs/LibFuzzer.html for more information

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length) {
	std::string str((char *)bytes, length);

	if (length > 0 && str[0] == 'H')
		if (length > 1 && str[1] == 'I')
			if (length > 2 && str[2] == '!')
				if (length > 14 && str == "HI!abcdefghijkl")
					abort();

	return EXIT_SUCCESS;
}
