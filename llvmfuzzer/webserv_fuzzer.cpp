#include <string.h>

#include <iostream>
#include <sstream>

#include "../src/config/Config.hpp"
#include "../src/config/JSON.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *bytes, size_t length) {
	std::string str((char *)bytes, length);

	std::stringstream config_stream(str);

	Config config;
	try
	{
		JSON json(config_stream);

		config.init(json);
	}
	catch (const Tokenizer::TokenException &e)
	{
		return EXIT_FAILURE;
	}
	catch (const Node::NodeException &e)
	{
		return EXIT_FAILURE;
	}
	catch (const JSON::JSONException &e)
	{
		return EXIT_FAILURE;
	}
	catch (const Config::ConfigException &e)
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
