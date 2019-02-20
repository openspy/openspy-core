#ifndef _OS_PROFILE_H
#define _OS_PROFILE_H
#include <string>
#include <jansson.h>
#include <OS/Date.h>
namespace OS {
	class Profile {
	public:
		Profile() { 
			id = 0;
			userid = 0;
			namespaceid = -1;
			deleted = false; 
			icquin = 0;
			zipcode = 0;
			sex = -1; 
			pic = 0;
			ooc = 0;
			ind = 0; 
			mar = 0;
			chc = 0; 
			i1 = 0; 
			birthday = Date(); 
			lon = 0.0; 
			lat = 0.0;
		};
		int id;
		int userid;
		std::string nick;
		std::string uniquenick;
		int namespaceid;
		bool deleted;

		std::string firstname;
		std::string lastname;
		int icquin;
		int zipcode;
		int sex;
		int pic; //picture id
		int ooc; //occupation id
		int ind; //income id
		int mar; //marriage id
		int chc; //child count
		int i1; //interests
		OS::Date birthday;

		float lon;
		float lat;

		std::string homepage;
		std::string countrycode;
		std::string videocardstring[2];
		std::string osstring; //operating system str
		std::string aim;
		std::string cdkey;
	};

	json_t *ProfileToJson(Profile profile);
	Profile LoadProfileFromJson(json_t *obj);
}
#endif //_OS_PROFILE_H