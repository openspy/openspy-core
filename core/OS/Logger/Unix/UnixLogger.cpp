#include <OS/OpenSpy.h>
#include <OS/Logger.h>

#include "UnixLogger.h"
#include <syslog.h>
namespace OS {
	UnixLogger::UnixLogger(const char *name) : Logger(name) {
		openlog(name, LOG_PID|LOG_CONS, LOG_USER);
	}
	UnixLogger::~UnixLogger() {
		closelog();
	}
	void UnixLogger::LogText(ELogLevel level, const char * format, va_list args) {
		int log_level = LOG_INFO;
		switch(level) {
			case ELogLevel_Debug:
				log_level = LOG_DEBUG;
			break;
			case ELogLevel_Warning:
				log_level = LOG_WARNING;
			break;
			case ELogLevel_Critical:
				log_level = LOG_CRIT;
			break;
			case ELogLevel_Info:
				log_level = LOG_INFO;
			break;
			case ELogLevel_Error:
				log_level = LOG_ERR;
			break;
			case ELogLevel_Auth:
				log_level = LOG_AUTH;
			break;
		}
		char buffer[4096];
		vsprintf(buffer, format, args);
		syslog(log_level, buffer);
	}
}