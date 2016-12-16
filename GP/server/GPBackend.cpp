#include <stdio.h>
#include "GPBackend.h"
namespace GP {
	void TryAddBuddy(int from_profileid, int to_profileid, const char *reason) {
		printf("Try Add: %d %d\n",from_profileid, to_profileid);
	}
	void AuthorizeBuddy(int adding_target, int adding_source) {

	}
}