#include <OS/Profile.h>

namespace OS {
	Profile LoadProfileFromJson(json_t *obj) {
		Profile ret;
		json_t *j;
		j = json_object_get(obj, "id");
		if(j && json_is_integer(j))
			ret.id = json_integer_value(j);

		j = json_object_get(obj, "userid");
		if(j && json_is_integer(j)) {
			ret.userid = json_integer_value(j);
		}
		else if(!j) {
			json_t *user_obj = json_object_get(obj, "user");
			j = json_object_get(user_obj, "id");
			if(j)
				ret.userid = json_integer_value(j);
		}

		j = json_object_get(obj, "namespaceid");
		if(j && json_is_integer(j))
			ret.namespaceid = json_integer_value(j);

		j = json_object_get(obj, "nick");
		if(j && json_is_string(j))
			ret.nick = json_string_value(j);

		j = json_object_get(obj, "uniquenick");
		if(j && json_is_string(j))
			ret.uniquenick = json_string_value(j);

		j = json_object_get(obj, "firstname");
		if(j && json_is_string(j))
			ret.firstname = json_string_value(j);

		j = json_object_get(obj, "lastname");
		if(j && json_is_integer(j))
			ret.lastname = json_string_value(j);

		j = json_object_get(obj, "homepage");
		if(j && json_is_string(j))
			ret.homepage = json_string_value(j);

		j = json_object_get(obj, "icquin");
		if(j && json_is_integer(j))
			ret.icquin = json_integer_value(j);
		else 
			ret.icquin = 0;

		j = json_object_get(obj, "sex");
		if(j && json_is_integer(j))
			ret.sex = json_integer_value(j);
		else 
			ret.sex = 0;

		j = json_object_get(obj, "zipcode");
		if(j && json_is_integer(j))
			ret.zipcode = json_integer_value(j);
		else 
			ret.zipcode = 0;

		j = json_object_get(obj, "pic");
		if(j && json_is_integer(j))
			ret.pic = json_integer_value(j);
		else 
			ret.pic = 0;

		j = json_object_get(obj, "ooc");
		if(j && json_is_integer(j))
			ret.ooc = json_integer_value(j);
		else 
			ret.ooc = 0;

		j = json_object_get(obj, "ind");
		if(j && json_is_integer(j))
			ret.ind = json_integer_value(j);
		else 
			ret.ind = 0;

		j = json_object_get(obj, "mar");
		if(j && json_is_integer(j))
			ret.mar = json_integer_value(j);
		else 
			ret.mar = 0;

		j = json_object_get(obj, "chc");
		if(j && json_is_integer(j))
			ret.chc = json_integer_value(j);
		else 
			ret.chc = 0;

		j = json_object_get(obj, "i1");
		if(j && json_is_integer(j))
			ret.i1 = json_integer_value(j);
		else 
			ret.i1 = 0;

		j = json_object_get(obj, "birthday");
		if(j)
			ret.birthday = OS::Date::GetDateFromJSON(j);
		else 
			ret.birthday = OS::Date();

		j = json_object_get(obj, "countrycode");
		if(j && json_is_string(j))
			ret.countrycode = json_string_value(j);

		j = json_object_get(obj, "videocard1string");
		if(j && json_is_string(j))
			ret.videocardstring[0] = json_string_value(j);

		j = json_object_get(obj, "videocard2string");
		if(j && json_is_string(j))
			ret.videocardstring[1] = json_string_value(j);

		j = json_object_get(obj, "osstring");
		if(j && json_is_string(j))
			ret.osstring = json_string_value(j);

		j = json_object_get(obj, "aim");
		if(j && json_is_string(j))
			ret.aim = json_string_value(j);

		j = json_object_get(obj, "lat");
		if(j && json_is_real(j))
			ret.lat = json_real_value(j);

		j = json_object_get(obj, "lon");
		if(j && json_is_real(j))
			ret.lon = json_real_value(j);

		j = json_object_get(obj, "deleted");
		if(j && json_is_boolean(j))
			ret.deleted = j == json_true();

		return ret;
	}
}