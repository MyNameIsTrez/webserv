#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// TODO: REMOVE
#define SERVER_ADDRESS "127.0.0.1"

#define SERVER_PORT 8080

#define MAX_RECEIVED_LEN 4096

void die(const char *fmt, ...)
{
    int errno_copy = errno;

    // Print out the fmt + args to stderr
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    fflush(stderr);

    // Print out error message if errno was set
    if (errno_copy != 0)
    {
        fprintf(stderr, "errno %d: %s\n", errno_copy, strerror(errno_copy));
        fflush(stderr);
    }

    exit(EXIT_FAILURE);
}

// Source: https://stackoverflow.com/q/41904221/13279557
ssize_t write_fully(int fd, const char *buf, size_t len)
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

/*
c++ client.cpp -Wall -Wextra -Werror -Wpedantic -Wshadow -Wfatal-errors -g -fsanitize=address,undefined && ./a.out 127.0.0.1

Code stolen from https://youtu.be/bdIiTxtMaKA
*/
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        die("Usage: %s <ip>", argv[0]);
    }

    int socket_fd;
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        die("socket");
    }

    // Set up socket struct
    sockaddr_in socket{};
    socket.sin_family = AF_INET;
    // Convert host (our) byte order to the network byte order as short (uint16_t)
    socket.sin_port = htons(SERVER_PORT);

    // Turn ip_str into an actual number, and store it in sin_addr
    // char *ip_str = argv[1];
    // if (inet_pton(AF_INET, ip_str, &socket.sin_addr) <= 0)
    if (inet_pton(AF_INET, SERVER_ADDRESS, &socket.sin_addr) <= 0)
    {
        // die("inet_pton with IP '%s'", ip_str);
        die("inet_pton with IP '%s'", SERVER_ADDRESS);
    }

    if (connect(socket_fd, (sockaddr *)&socket, sizeof(socket)) < 0)
    {
        die("connect");
    }

    // We're connected; prepare sent message
    // Message asks to get / (root) homepage
    // const char *sent = "GET / HTTP/1.1\r\n\r\n";
    const char *sent = ".";
    size_t sent_len = strlen(sent);

    // write() its behavior could be weird with a sent_len of 0
    // Source: https://stackoverflow.com/a/41970485/13279557
    if (sent_len == 0)
    {
        die("sent_len was 0");
    }

    // Send the request, making sure that every byte was sent
    if (write_fully(socket_fd, sent, sent_len) < 0)
    {
        die("write_fully");
    }

    // NULL-terminate received string
    char received[MAX_RECEIVED_LEN + 1]{};

    // Keep attempting to read, until there is no more data to receive (0),
    // or an error is returned (-1)
    // read() will block the program, waiting for more stuff to read
    ssize_t bytes_read;
    while ((bytes_read = read(socket_fd, received, MAX_RECEIVED_LEN)) > 0)
    {
        printf("%s", received);
        bzero(received, MAX_RECEIVED_LEN + 1);
    }
    if (bytes_read < 0)
    {
        die("read");
    }

    return EXIT_SUCCESS;
}
