#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// clear && c++ fork_kill.cpp && ./a.out
int main()
{
    pid_t pid = fork();
    if (pid == 0)
    {
        return 0;
    }
    int wstatus;
    kill(pid, SIGTERM);
    waitpid(pid, &wstatus, 0);
    while (1)
    {
        std::cout << "kill returned " << kill(pid, SIGTERM) << std::endl;
        sleep(1);
    }
}
