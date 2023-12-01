#include <assert.h>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>

// clear && c++ -std=c++2b filesystem_remove.cpp && ./a.out
int main()
{
    std::string path = "foo.txt";

    for (int round = 0; round < 100; round++)
    {
        std::ofstream outfile;
        outfile.open(path, std::fstream::out);

        for (int i = 0; i < 1000; i++)
        {
            outfile << "foo" << std::endl;
        }

        // sleep(1);

        std::filesystem::remove(path);

        DIR *dirp = opendir(".");
        assert(dirp != NULL);

        bool seen_footxt = false;

        const dirent *dp;
        while ((dp = readdir(dirp)) != NULL)
        {
            std::string name = dp->d_name;

            // std::cout << name << std::endl;

            // Tests whether filesystem::remove() is async
            if (name == "foo.txt")
            {
                seen_footxt = true;
            }
        }

        assert(!seen_footxt);

        closedir(dirp);
    }
}
