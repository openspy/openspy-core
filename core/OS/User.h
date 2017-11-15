#ifndef _OS_USER_H
#define _OS_USER_H
#include <string>
#include <jansson.h>
namespace OS {
	class User {
	public:
		User() {
			id = 0;
			partnercode = 0;
			videocard_ram[0] = 0;
			videocard_ram[1] = 0;
			cpu_speed = 0;
			cpu_brandid = 0;
			connectionspeed = 0;
			connectionid = 0;
			hasnetwork = false;
			email_verified = false;
			publicmask = 0;
			deleted = false;
		};
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
	};

	User LoadUserFromJson(json_t *obj);
}
#endif //_OS_USER_H