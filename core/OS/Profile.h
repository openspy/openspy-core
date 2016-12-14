#ifndef _OS_PROFILE_H
#define _OS_PROFILE_H
#include <string>
#include <jansson.h>
namespace OS {
	typedef struct {
		int id;
		int userid;
		std::string nick;
		std::string uniquenick;
		int namespaceid;
		bool deleted;
	} Profile;

	Profile LoadProfileFromJson(json_t *obj);
}
#endif //_OS_PROFILE_H