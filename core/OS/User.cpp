#include <OS/User.h>

namespace OS {
	User LoadUserFromJson(json_t *obj) {
		User ret;
		json_t *j;
		j = json_object_get(obj, "id");
		if(j)
			ret.id = json_integer_value(j);

		j = json_object_get(obj, "email");
		if(j)
			ret.email = json_string_value(j);

		j = json_object_get(obj, "partnercode");
		if(j)
			ret.partnercode = json_integer_value(j);

		j = json_object_get(obj, "publicmask");
		if(j && json_is_integer(j))
			ret.publicmask = json_integer_value(j);
		else 
			ret.publicmask = 0;

		ret.email_verified = json_object_get(obj, "email_verified") == json_true();
		ret.deleted = json_object_get(obj, "deleted") == json_true();

		return ret;
	}
}