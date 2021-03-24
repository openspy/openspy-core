#ifndef _OS_WIN32LOGGER_H
#define _OS_WIN32LOGGER_H
#include <OS/OpenSpy.h>
namespace OS {
	class Win32Logger : public Logger {
		public:
			Win32Logger(const char *name);
			~Win32Logger();
			void LogText(ELogLevel level, const char * format, va_list args);
		protected:
			const char *mp_name;
	};
}
#endif //_OS_UNIXLOGGER_H