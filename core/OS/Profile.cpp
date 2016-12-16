#include <OS/Profile.h>

namespace OS {
	Profile LoadProfileFromJson(json_t *obj) {
		Profile ret;
		json_t *j;
		j = json_object_get(obj, "id");
		if(j && json_is_integer(j))
			ret.id = json_integer_value(j);

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

		j = json_object_get(obj, "deleted");
		if(j && json_is_boolean(j))
			ret.deleted = j == json_true();

		return ret;
	}
}