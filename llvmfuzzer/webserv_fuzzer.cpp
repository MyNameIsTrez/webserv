#include <string.h>

#include <iostream>
#include <sstream>

#include "../src/Config.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length) {
	std::string str((char *)bytes, length);

	std::stringstream config_stream(str);
	Config config;
	try
	{
		config.init(config_stream);
		// throw InvalidLineException();
	}
	catch (const Config::ConfigException &e)
	{
		// abort();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
