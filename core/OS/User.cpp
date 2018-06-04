#include <OS/User.h>

namespace OS {
	User LoadUserFromJson(json_t *obj) {
		User ret;
		json_t *j;
		j = json_object_get(obj, "id");
		if(j)
			ret.id = (int)json_integer_value(j);

		j = json_object_get(obj, "email");
		if(j)
			ret.email = json_string_value(j);

		j = json_object_get(obj, "partnercode");
		if(j)
			ret.partnercode = (int)json_integer_value(j);

		j = json_object_get(obj, "videocard1ram");
		if(j)
			ret.videocard_ram[0] = (int)json_integer_value(j);

		j = json_object_get(obj, "videocard2ram");
		if(j)
			ret.videocard_ram[1] = (int)json_integer_value(j);

		j = json_object_get(obj, "cpuspeed");
		if(j)
			ret.cpu_speed = (int)json_integer_value(j);

		j = json_object_get(obj, "cpubrandid");
		if(j)
			ret.cpu_brandid = (int)json_integer_value(j);

		j = json_object_get(obj, "connectionspeed");
		if(j)
			ret.connectionspeed = (int)json_integer_value(j);

		j = json_object_get(obj, "connectionid");
		if(j)
			ret.connectionid = (int)json_integer_value(j);

		j = json_object_get(obj, "hasnetwork");
		if(j)
			ret.connectionid = j == json_true();

		j = json_object_get(obj, "publicmask");
		if(j && json_is_integer(j))
			ret.publicmask = (int)json_integer_value(j);
		else 
			ret.publicmask = 0;

		ret.email_verified = json_object_get(obj, "email_verified") == json_true();
		ret.deleted = json_object_get(obj, "deleted") == json_true();

		return ret;
	}
}