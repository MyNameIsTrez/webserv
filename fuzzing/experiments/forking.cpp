#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <stdbool.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void run(const char *buf, int len)
{
    (void)buf;
    (void)len;

    std::cout << "In run()" << std::endl;
}

/*
Run forking.sh to run this
*/
int main(int argc, char *argv[])
{
    std::cout << "Started program" << std::endl;

    std::cout << "Going to fork" << std::endl;

    pid_t pid = fork();
    assert(pid != -1);
    if (pid == 0)
    {
        std::string buf(std::istreambuf_iterator<char>(std::cin), {});

        run(buf.data(), buf.length());

        return EXIT_SUCCESS;
    }

    std::cout << "Inside of parent" << std::endl;

    std::cout << "argc is " << argc << std::endl;
    // This mfer used to prevent coverage information from being generated, as I forgot to pass the .json config file as the 2nd arg to afl-cov
    assert(argc == 2);
    if (argc != 2)
    {
        std::ofstream f("/src/fuzzing/wtf.txt");
        f << "wtf" << std::endl;
    }
    if (argc != 2)
    {
        abort();
    }

    int wstatus;
    assert(waitpid(pid, &wstatus, 0) != -1);

    return EXIT_SUCCESS;
}
