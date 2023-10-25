#include "Logger.hpp"

#ifdef SUPPRESS_LOGGING
Logger::Level Logger::level = Logger::NONE;
#else
Logger::Level Logger::level = Logger::DEBUG;
#endif
