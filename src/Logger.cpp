#include "Logger.hpp"

namespace L = Logger;

#ifdef SUPPRESS_LOGGING
L::Level L::level = L::Level::NONE;
#else
L::Level L::level = L::Level::DEBUG;
#endif
