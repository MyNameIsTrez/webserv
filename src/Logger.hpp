#pragma once

#include <iostream>
#include <string>

namespace Logger
{
enum class Level
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    NONE
};

extern Level level;

template <typename T> void debug(const T &message)
{
    if (level <= Level::DEBUG)
        std::cout << "[DEBUG]: " << message << std::endl;
}
template <typename T> void info(const T &message)
{
    if (level <= Level::INFO)
        std::cout << "[INFO]: " << message << std::endl;
}
template <typename T> void warning(const T &message)
{
    if (level <= Level::WARNING)
        std::cout << "[WARNING]: " << message << std::endl;
}
template <typename T> void error(const T &message)
{
    if (level <= Level::ERROR)
        std::cout << "[ERROR]: " << message << std::endl;
}
}; // namespace Logger
