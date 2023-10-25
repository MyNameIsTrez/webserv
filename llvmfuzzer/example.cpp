// clear && clang++ -fsanitize=fuzzer,undefined,address -fprofile-instr-generate -fcoverage-mapping example.cpp && ./a.out
// clear && clang++ -fsanitize=fuzzer,undefined,memory -fprofile-instr-generate -fcoverage-mapping example.cpp && ./a.out
//
// See https://llvm.org/docs/LibFuzzer.html for more information

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length) {
	if (length > 0 && bytes[0] == 'H')
		if (length > 1 && bytes[1] == 'I')
			if (length > 2 && bytes[2] == '!')
				if (length > 14 && strcmp((char *)bytes, "HI!abcdefghijkl") == 0)
					abort();

	return EXIT_SUCCESS;
}
