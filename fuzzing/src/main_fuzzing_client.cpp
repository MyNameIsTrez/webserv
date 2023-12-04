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

static void stop_running_server_handler(int num)
{
    (void)num;
    Server::running = false;
}

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

static void run(const char *buf, int len)
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
    assert(socket_fd != -1);

    // Prevents "bind: Address already in use" error
    int option = 1; // "the parameter should be non-zero to enable a boolean option"
    assert(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) != -1);

    // Set up socket struct
    sockaddr_in socket{};
    socket.sin_family = AF_INET;
    // Convert host (our) byte order to the network byte order as short (uint16_t)
    socket.sin_port = htons(SERVER_PORT);

    assert(inet_pton(AF_INET, SERVER_ADDRESS, &socket.sin_addr) > 0);

    while (connect(socket_fd, (sockaddr *)&socket, sizeof(socket)) == -1)
    {
        // Retry as the server (presumably) hasn't called listen() yet
        usleep(100); // 0.1 ms
    }

    // Send the request, making sure that every byte was sent
    std::cout << "Will now call write() in a loop..." << std::endl;
    if (write_fully(socket_fd, buf, len) < 0)
    {
        std::cout << "write() returned -1" << std::endl;
        assert(close(socket_fd) != -1);
        return;
    }

    // NULL-terminate received string
    char received[MAX_RECEIVED_LEN + 1]{};

    // Keep attempting to read, until there is no more data to receive (0),
    // or an error is returned (-1)
    // read() will block the program, waiting for more stuff to read
    ssize_t bytes_read;
    std::cout << "Will now call read() in a loop..." << std::endl;
    while ((bytes_read = read(socket_fd, received, MAX_RECEIVED_LEN)) > 0)
    {
        std::cout << "bytes_read is " << bytes_read << std::endl;
        printf("Read these bytes: '%s'\n", received);
        bzero(received, MAX_RECEIVED_LEN + 1);
    }
    // It often returns -1 "Connection reset by peer", but we don't care and continue
    std::cout << "bytes_read is " << bytes_read << std::endl;

    assert(close(socket_fd) != -1);
}

int main(int argc, char *argv[])
{
    std::cout << "Started program" << std::endl;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    unsigned char *buf = (unsigned char *)"";
#ifdef AFL
    buf = __AFL_FUZZ_TESTCASE_BUF;
#else
    std::string str_buf(std::istreambuf_iterator<char>(std::cin), {});
    buf = (unsigned char *)str_buf.data();
#endif

    assert(signal(SIGUSR1, stop_running_server_handler) != SIG_ERR);

    pid_t server_pid = getpid();

    std::cout << "Going to fork" << std::endl;

    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0)
    {
        assert(signal(SIGUSR1, SIG_IGN) != SIG_ERR);

#if defined AFL || defined GCOV
        // __extension__ is necessary when using -Wpedantic
        while (__extension__ __AFL_LOOP(2))
#endif
        {
#ifdef AFL
            int len = __AFL_FUZZ_TESTCASE_LEN;
#else
            int len = str_buf.length();
#endif

            run((const char *)buf, len);

            // TODO: The reason it hangs here is maybe because the server isn't told to stop running?
            std::cout << "At the end of the __AFL_LOOP" << std::endl;
        }

        std::cout << "Telling the server to stop running..." << std::endl;
        // We don't care whether kill() returned -1
        kill(server_pid, SIGUSR1);

        return EXIT_SUCCESS;
    }

    std::cout << "Inside of parent" << std::endl;

    std::cout << "argc is " << argc << std::endl;
    assert(argc == 2);

    std::string file_name(argv[1]);

    std::cout << "Opening config file '" << file_name << "'" << std::endl;
    std::ifstream config_file(file_name);

    assert(config_file.is_open());

    assert(std::filesystem::file_size(file_name) <= 420420);

    JSON json(config_file, 5);

    Config config(json);

    Server server(config);

    std::cout << "Running server..." << std::endl;
    server.run();

    // TODO: It might be faster if we didn't have to wait,
    // TODO: by doing something that tells the child to be reaped instantly?
    int wstatus;
    assert(waitpid(pid, &wstatus, 0) != -1);

    return EXIT_SUCCESS;
}
