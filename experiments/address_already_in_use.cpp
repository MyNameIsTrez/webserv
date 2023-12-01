#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SERVER_PORT 8080
#define CONNECTION_QUEUE_LEN 420

// This function is necessary because assert(), abort() and exit()
// will cause "bind: Address already in use" the next time this program is run
static void check(bool live)
{
    if (!live)
    {
        // kill(0, ...) will also send the signal to the parent, so ignore it
        signal(SIGQUIT, SIG_IGN);

        assert(kill(0, SIGQUIT) != -1);

        std::cout << "I lived!" << std::endl;

        exit(EXIT_FAILURE);
    }
}

// If a child creates a socket and calls bind() on it, that child will hold that socket for seemingly forever if the parent happens to abort! :(
// If the parent however calls kill() on the child before aborting, "bind: Address already in use" is prevented.
//
// c++ address_already_in_use.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out
int main(void)
{
    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0)
    {
        int server_fd;
        if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        // Prevents "bind: Address already in use" error
        int option = 1; // "the parameter should be non-zero to enable a boolean option"
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
        {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        sockaddr_in servaddr{};
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(SERVER_PORT);

        if ((bind(server_fd, (sockaddr *)&servaddr, sizeof(servaddr))) < 0)
        {
            perror("bind");
            exit(EXIT_FAILURE);
        }

        if ((listen(server_fd, CONNECTION_QUEUE_LEN)) < 0)
        {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        // This doesn't cause "Address already in use", so is fine
        // abort();

        while (true)
        {
        }
    }

    // Make sure the child has the time to call bind()
    sleep(3);

    // This prevents "Address already in use"
    // assert(kill(pid, SIGTERM) != -1);

    // This prevents "Address already in use"
    check(false);

    // This DOES NOT prevent "Address already in use"
    // abort();

    return EXIT_SUCCESS;
}
