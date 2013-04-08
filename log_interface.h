#ifndef _LOG_INTERFACE_H_
#define _LOG_INTERFACE_H_

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <log4cplus/consoleappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/helpers/pointer.h>
#include <log4cplus/ndc.h>

#define C_LOG_INST (log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("calypso")))

#define C_TRACE(logFmt, ...) LOG4CPLUS_TRACE_FMT(C_LOG_INST, logFmt, __VA_ARGS__)
#define C_DEBUG(logFmt, ...) LOG4CPLUS_DEBUG_FMT(C_LOG_INST, logFmt, __VA_ARGS__)
#define C_INFO(logFmt, ...) LOG4CPLUS_INFO_FMT(C_LOG_INST, logFmt, __VA_ARGS__)
#define C_WARN(logFmt, ...) LOG4CPLUS_WARN_FMT(C_LOG_INST, logFmt, __VA_ARGS__)
#define C_ERROR(logFmt, ...) LOG4CPLUS_ERROR_FMT(C_LOG_INST, logFmt, __VA_ARGS__)
#define C_FATAL(logFmt, ...) LOG4CPLUS_FATAL_FMT(C_LOG_INST, logFmt, __VA_ARGS__)

#define C_PUSH_NDC(name) log4cplus::getNDC().push(LOG4CPLUS_TEXT(name))
#define C_CLEAR_NDC() log4cplus::getNDC().clear()

#endif
