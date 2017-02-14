#ifndef _OS_LOGGER_H
#define _OS_LOGGER_H
#include <OS/OpenSpy.h>
/*
	The core logger for openspy

	Do not call this directly, call OS::Log instead

	There should only be 1 instance of this per process existing within openspy itself
*/
namespace OS {
	enum ELogLevel {
		ELogLevel_Debug,
		ELogLevel_Info,
		ELogLevel_Warning,
		ELogLevel_Critical,
		ELogLevel_Error,
		ELogLevel_Auth
	};
	class Logger {
		public:
			Logger(const char *name) { mp_name = name; };
			virtual ~Logger() {};
			virtual void LogText(ELogLevel level, const char * format, va_list args) = 0;
		protected:
			const char *mp_name;
	};
}
#endif //_OS_LOGGER_H