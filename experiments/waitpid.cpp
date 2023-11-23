#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// clear && c++ waitpid.cpp && ./a.out
int main()
{
    if (fork() == 0)
    {
        sleep(3);
        return 0;
    }

    while (true)
    {
        int wstatus;
        pid_t pid = waitpid(-1, &wstatus, WNOHANG);
        std::cout << "wstatus: " << wstatus << std::endl;
        std::cout << "pid: " << pid << std::endl;
        if (pid > 0)
            break;
        sleep(1);
    }
}
