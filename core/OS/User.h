#ifndef _OS_USER_H
#define _OS_USER_H
#include <string>
#include <jansson.h>
namespace OS {
	typedef struct {
		int id;
		std::string email;
		int partnercode;

		//below not searchable
		std::string password;
		int videocard_ram[2];
		int cpu_speed;
		int cpu_brandid;
		int connectionspeed;
		int connectionid;
		bool hasnetwork;

		bool email_verified;
		int publicmask; //appears as user var in GP
		bool deleted;
	} User;

	User LoadUserFromJson(json_t *obj);
}
#endif //_OS_USER_H