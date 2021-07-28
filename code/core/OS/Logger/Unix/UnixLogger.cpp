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
		const char *level_str = "NA";
		switch(level) {
			case ELogLevel_Debug:
				level_str = "DBG";
				log_level = LOG_DEBUG;
			break;
			case ELogLevel_Warning:
				level_str = "WARN";
				log_level = LOG_WARNING;
			break;
			case ELogLevel_Critical:
				level_str = "CRIT";
				log_level = LOG_CRIT;
			break;
			case ELogLevel_Info:
				level_str = "INFO";
				log_level = LOG_INFO;
			break;
			case ELogLevel_Error:
				level_str = "ERR";
				log_level = LOG_ERR;
			break;
			case ELogLevel_Auth:
				level_str = "AUTH";
				log_level = LOG_AUTH;
			break;
		}
		vsyslog(log_level, format, args);
	}
}