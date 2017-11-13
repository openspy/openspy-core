#include <string.h>
const char* inet_ntop(int af, const void *src, char *dst, size_t size) {
	strcpy(dst, "0.0.0.0");
	return "0.0.0.0";
}