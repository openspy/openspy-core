#include <OS/Profile.h>

namespace OS {
	Profile LoadProfileFromJson(json_t *obj) {
		Profile ret;
		json_t *j;
		j = json_object_get(obj, "id");
		if(j)
			ret.id = json_integer_value(j);

		j = json_object_get(obj, "namespaceid");
		if(j)
			ret.namespaceid = json_integer_value(j);

		j = json_object_get(obj, "nick");
		if(j)
			ret.nick = json_string_value(j);

		j = json_object_get(obj, "uniquenick");
		if(j)
			ret.uniquenick = json_string_value(j);

		j = json_object_get(obj, "deleted");
		if(j)
			ret.deleted = j == json_true();

		return ret;
	}
}