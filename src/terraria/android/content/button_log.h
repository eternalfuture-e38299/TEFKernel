#ifndef TEFKERNEL_BUTTON_LOG_H
#define TEFKERNEL_BUTTON_LOG_H

#include "internal/log.h"

#define BUTTON_LOG_INFO(...) TEKLOG_INFO(__VA_ARGS__)
#define BUTTON_LOG_WARN(...) TEKLOG_WARN(__VA_ARGS__)
#define BUTTON_LOG_ERROR(...) TEKLOG_ERROR(__VA_ARGS__)

#endif
