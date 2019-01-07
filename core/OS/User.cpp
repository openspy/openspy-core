#include <OS/User.h>

namespace OS {
	json_t *UserToJson(User user) {
		json_t *user_obj = json_object();
		json_object_set_new(user_obj, "id", json_integer(user.id));

		if(user.email.length() > 0)
			json_object_set_new(user_obj, "email", json_string(user.email.c_str()));

		if (user.password.length())
			json_object_set_new(user_obj, "password", json_string(user.password.c_str()));

		json_object_set_new(user_obj, "videocard1ram", json_integer(user.videocard_ram[0]));
		json_object_set_new(user_obj, "videocard2ram", json_integer(user.videocard_ram[1]));

		json_object_set_new(user_obj, "cpuspeed", json_integer(user.cpu_speed));
		json_object_set_new(user_obj, "cpubrandid", json_integer(user.cpu_brandid));
		json_object_set_new(user_obj, "connectionspeed", json_integer(user.connectionspeed));
		json_object_set_new(user_obj, "hasnetwork", user.hasnetwork ? json_true() : json_false());
		json_object_set_new(user_obj, "publicmask", json_integer(user.publicmask));
		if (user.partnercode != -1) {
			json_object_set_new(user_obj, "partnercode", json_integer(user.partnercode));
		}

		return user_obj;
	}
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