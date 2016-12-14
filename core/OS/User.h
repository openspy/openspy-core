#ifndef _OS_USER_H
#define _OS_USER_H
#include <string>
#include <jansson.h>
namespace OS {
	typedef struct {
		int id;
		std::string email;
		int partnercode;
		bool email_verified;
		bool deleted;
	} User;

	User LoadUserFromJson(json_t *obj);
}
#endif //_OS_USER_H