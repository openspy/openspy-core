#include <OS/Profile.h>

namespace OS {
	json_t *ProfileToJson(Profile profile) {
		json_t *profile_obj = json_object();
		json_object_set_new(profile_obj, "id", json_integer(profile.id));
		//profile parameters
		if (profile.nick.length())
			json_object_set_new(profile_obj, "nick", json_string(profile.nick.c_str()));

		if (profile.uniquenick.length())
			json_object_set_new(profile_obj, "uniquenick", json_string(profile.uniquenick.c_str()));

		if (profile.firstname.length())
			json_object_set_new(profile_obj, "firstname", json_string(profile.firstname.c_str()));

		if (profile.lastname.length())
			json_object_set_new(profile_obj, "lastname", json_string(profile.lastname.c_str()));

		if (profile.icquin)
			json_object_set_new(profile_obj, "icquin", json_integer(profile.icquin));

		if (profile.zipcode)
			json_object_set_new(profile_obj, "zipcode", json_integer(profile.zipcode));

		if (profile.sex != -1)
			json_object_set_new(profile_obj, "sex", json_integer(profile.sex));

		if (profile.pic != 0)
			json_object_set_new(profile_obj, "pic", json_integer(profile.pic));
		if (profile.ooc != 0)
			json_object_set_new(profile_obj, "ooc", json_integer(profile.ooc));
		if (profile.ind != 0)
			json_object_set_new(profile_obj, "ind", json_integer(profile.ind));
		if (profile.mar != 0)
			json_object_set_new(profile_obj, "mar", json_integer(profile.mar));
		if (profile.chc != 0)
			json_object_set_new(profile_obj, "chc", json_integer(profile.chc));
		if (profile.i1 != 0)
			json_object_set_new(profile_obj, "i1", json_integer(profile.i1));

		if (profile.birthday.GetYear() != 0)
			json_object_set_new(profile_obj, "birthday", profile.birthday.GetJson());

		if (profile.countrycode.length() > 0) {
			json_object_set_new(profile_obj, "countrycode", json_string(profile.countrycode.c_str()));
		}

		if (profile.lon)
			json_object_set_new(profile_obj, "lon", json_real(profile.lon));
		if (profile.lat)
			json_object_set_new(profile_obj, "lat", json_real(profile.lat));


		if (profile.namespaceid != -1)
			json_object_set_new(profile_obj, "namespaceid", json_integer(profile.namespaceid));
		return profile_obj;
	}
	Profile LoadProfileFromJson(json_t *obj) {
		Profile ret;
		json_t *j;
		j = json_object_get(obj, "id");
		if(j && json_is_integer(j))
			ret.id = (int)(int)json_integer_value(j);

		j = json_object_get(obj, "userid");
		if(j && json_is_integer(j)) {
			ret.userid = (int)json_integer_value(j);
		}
		else if(!j) {
			json_t *user_obj = json_object_get(obj, "user");
			j = json_object_get(user_obj, "id");
			if(j)
				ret.userid = (int)json_integer_value(j);
		}

		j = json_object_get(obj, "namespaceid");
		if(j && json_is_integer(j))
			ret.namespaceid = (int)json_integer_value(j);

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
		if(j && json_is_string(j))
			ret.lastname = json_string_value(j);

		j = json_object_get(obj, "homepage");
		if(j && json_is_string(j))
			ret.homepage = json_string_value(j);

		j = json_object_get(obj, "icquin");
		if(j && json_is_integer(j))
			ret.icquin = (int)json_integer_value(j);
		else 
			ret.icquin = 0;

		j = json_object_get(obj, "sex");
		if(j && json_is_integer(j))
			ret.sex = (int)json_integer_value(j);
		else 
			ret.sex = 0;

		j = json_object_get(obj, "zipcode");
		if(j && json_is_integer(j))
			ret.zipcode = (int)json_integer_value(j);
		else 
			ret.zipcode = 0;

		j = json_object_get(obj, "pic");
		if(j && json_is_integer(j))
			ret.pic = (int)json_integer_value(j);
		else 
			ret.pic = 0;

		j = json_object_get(obj, "ooc");
		if(j && json_is_integer(j))
			ret.ooc = (int)json_integer_value(j);
		else 
			ret.ooc = 0;

		j = json_object_get(obj, "ind");
		if(j && json_is_integer(j))
			ret.ind = (int)json_integer_value(j);
		else 
			ret.ind = 0;

		j = json_object_get(obj, "mar");
		if(j && json_is_integer(j))
			ret.mar = (int)json_integer_value(j);
		else 
			ret.mar = 0;

		j = json_object_get(obj, "chc");
		if(j && json_is_integer(j))
			ret.chc = (int)json_integer_value(j);
		else 
			ret.chc = 0;

		j = json_object_get(obj, "i1");
		if(j && json_is_integer(j))
			ret.i1 = (int)json_integer_value(j);
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
			ret.lat = (float)json_real_value(j);

		j = json_object_get(obj, "lon");
		if(j && json_is_real(j))
			ret.lon = (float)json_real_value(j);

		j = json_object_get(obj, "deleted");
		if(j && json_is_boolean(j))
			ret.deleted = j == json_true();

		return ret;
	}
}