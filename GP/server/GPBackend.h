#ifndef _GP_BACKEND_H
#define _GP_BACKEND_H
namespace GP {
	void TryAddBuddy(int from_profileid, int to_profileid, const char *reason);
	void AuthorizeBuddy(int adding_target, int adding_source);
}
#endif //_GP_BACKEND_H