#include "../../src/Server.hpp"
#include "../../src/config/Config.hpp"
#include "../../src/config/JSON.hpp"

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <filesystem>
#include <signal.h>
#include <stdbool.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_RECEIVED_LEN 4096

#if defined AFL || defined GCOV
__AFL_FUZZ_INIT()
#endif

// Source: https://stackoverflow.com/q/41904221/13279557
static ssize_t write_fully(int fd, const char *buf, size_t len)
{
    while (len > 0)
    {
        ssize_t bytes_written = write(fd, buf, len);
        if (bytes_written < 0)
        {
            // Error
            return bytes_written;
        }
        buf += bytes_written;
        len -= bytes_written;
    }
    // Success
    return 0;
}

// This function is necessary because assert(), abort() and exit()
// will cause "bind: Address already in use" the next time this program is run
static void check(bool live)
{
    if (!live)
    {
        // kill(0, ...) will also send the signal to the parent, so ignore it
        signal(SIGQUIT, SIG_IGN);

        assert(kill(0, SIGQUIT) != -1);

        exit(EXIT_FAILURE);
    }
}

static void run(char *buf, int len)
{
    // write() its behavior could be weird with a len of 0
    // Source: https://stackoverflow.com/a/41970485/13279557
    if (len <= 0)
    {
        return;
    }

    printf("buf is '%s'\n", buf);
    printf("len is %d\n", len);

    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(socket_fd != -1);

    // Prevents "bind: Address already in use" error
    int option = 1; // "the parameter should be non-zero to enable a boolean option"
    check(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) != -1);

    // Set up socket struct
    sockaddr_in socket{};
    socket.sin_family = AF_INET;
    // Convert host (our) byte order to the network byte order as short (uint16_t)
    socket.sin_port = htons(SERVER_PORT);

    check(inet_pton(AF_INET, SERVER_ADDRESS, &socket.sin_addr) > 0);

    if (connect(socket_fd, (sockaddr *)&socket, sizeof(socket)) == -1)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    // check(connect(socket_fd, (sockaddr *)&socket, sizeof(socket)) != -1);

    // Send the request, making sure that every byte was sent
    check(write_fully(socket_fd, buf, len) >= 0);

    // NULL-terminate received string
    char received[MAX_RECEIVED_LEN + 1]{};

    std::cout << "foo" << std::endl;

    // Keep attempting to read, until there is no more data to receive (0),
    // or an error is returned (-1)
    // read() will block the program, waiting for more stuff to read
    ssize_t bytes_read;
    while ((bytes_read = read(socket_fd, received, MAX_RECEIVED_LEN)) > 0)
    {
        printf("Read these bytes: '%s'\n", received);
        bzero(received, MAX_RECEIVED_LEN + 1);
    }
    std::cout << "bar" << std::endl;
    check(bytes_read >= 0);

    check(close(socket_fd) != -1);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    std::cout << "Started program" << std::endl;

    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0)
    {
        std::string file_name("fuzzing/fuzzing_client_webserv.json");

        std::ifstream config_file(file_name);

        assert(config_file.is_open());

        assert(std::filesystem::file_size(file_name) <= 420420);

        JSON json(config_file, 5);

        Config config(json);

        Server server(config);

        server.run();

        return EXIT_SUCCESS;
    }

    std::cout << "Running server" << std::endl;

    // Sleep so connect() won't fail if the parent hasn't called listen() yet
    // TODO: Make this way lower to speed up fuzzing, somehow!
    usleep(10000); // 10 ms

    unsigned char *buf = (unsigned char *)"";
#ifdef AFL
    buf = __AFL_FUZZ_TESTCASE_BUF;
#else
    std::string str_buf(std::istreambuf_iterator<char>(std::cin), {});
    buf = (unsigned char *)str_buf.data();
#endif

#if defined AFL || defined GCOV
    // __extension__ is necessary when using -Wpedantic
    while (__extension__ __AFL_LOOP(10000))
#endif
    {
#ifdef AFL
        int len = __AFL_FUZZ_TESTCASE_LEN;
#else
        int len = str_buf.length();
#endif

        run((char *)buf, len);
    }

    std::cout << "Killing server" << std::endl;

    check(kill(pid, SIGTERM) != -1);
    int wstatus;
    check(wait(&wstatus) != -1);

    return EXIT_SUCCESS;
}
