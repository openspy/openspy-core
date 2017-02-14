#ifndef _OS_UNIXLOGGER_H
#define _OS_UNIXLOGGER_H
#include <OS/OpenSpy.h>
namespace OS {
	class UnixLogger : public Logger {
		public:
			UnixLogger(const char *name);
			~UnixLogger();
			void LogText(ELogLevel level, const char * format, va_list args);
		protected:
			const char *mp_name;
	};
}
#endif //_OS_UNIXLOGGER_H