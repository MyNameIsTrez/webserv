#include "Logger.hpp"

#ifdef SUPPRESS_LOGGING
Logger::Level Logger::level = Logger::Level::NONE;
#else
Logger::Level Logger::level = Logger::Level::DEBUG;
#endif
