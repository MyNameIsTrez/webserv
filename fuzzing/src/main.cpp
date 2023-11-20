#include "../../src/config/Config.hpp"
#include "../../src/config/JSON.hpp"

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined AFL || defined GCOV
__AFL_FUZZ_INIT()
#endif

static void run(int argc, char *argv[], char *buf)
{
    (void)argc;
    (void)argv;

    std::stringstream config_stream(buf);

    try
    {
        JSON json(config_stream);

        Config config(json);
    }
    catch (const Tokenizer::TokenException &e)
    {
    }
    catch (const Node::NodeException &e)
    {
    }
    catch (const JSON::JSONException &e)
    {
    }
    catch (const Config::ConfigException &e)
    {
    }
}

int main(int argc, char *argv[])
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    printf("Started program\n");

    unsigned char *buf = (unsigned char *)"";
#ifdef AFL
    argc = 2;
    buf = __AFL_FUZZ_TESTCASE_BUF;
#elif defined GCOV || defined CTMIN
    argc = 2;
    std::string str_buf(std::istreambuf_iterator<char>(std::cin), {});
    buf = (unsigned char *)str_buf.data();
#endif

#if defined AFL || defined GCOV
    // __extension__ is necessary when using -Wpedantic
    while (__extension__ __AFL_LOOP(10000))
#endif
    {
        run(argc, argv, (char *)buf);
    }

    return EXIT_SUCCESS;
}
