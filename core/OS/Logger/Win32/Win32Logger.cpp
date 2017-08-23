#include <OS/OpenSpy.h>
#include <OS/Logger.h>

#include "Win32Logger.h"
#include <stdio.h>
namespace OS {
	Win32Logger::Win32Logger(const char *name) : Logger(name) {
		
	}
	Win32Logger::~Win32Logger() {
		
	}
	void Win32Logger::LogText(ELogLevel level, const char * format, va_list args) {
		vprintf(format, args);
		printf("\n");
	}
}