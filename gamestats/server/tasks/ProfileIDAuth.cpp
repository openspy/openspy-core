#include "tasks.h"
#include <sstream>
#include <OS/HTTP.h>
#include <server/GSPeer.h>
namespace GS {
	bool Perform_ProfileIDAuth(PersistBackendRequest request, TaskThreadData *thread_data) {
		json_t *send_json = json_object(), *profile_obj = json_object();

		json_object_set_new(profile_obj, "id", json_integer(request.profileid));

		json_object_set_new(send_json, "profileLookup", profile_obj);

		json_object_set_new(send_json, "client_response", json_string(request.game_instance_identifier.c_str()));

		json_object_set_new(send_json, "session_key", json_integer(request.modified_since));

		std::string url = std::string(OS::g_webServicesURL) + "/v1/Persist/Auth/SessionKeyAuth";

		OS::HTTPClient client(url);
		PersistBackendResponse resp_data;

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Post(json_data, request.mp_peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;
		json_t *error_obj = json_object_get(send_json, "error");
		json_t *profile;
		OS::Profile auth_profile;
		OS::User auth_user;
		TaskShared::AuthData auth_data;
		if (resp.status_code == 0 || error_obj) {
			auth_data.response_code = TaskShared::LOGIN_RESPONSE_INVALID_PASSWORD;
			success = false;
		}
		else {
			auth_data.response_code = TaskShared::LOGIN_RESPONSE_SUCCESS;
			profile = json_object_get(send_json, "profile");
			if (profile) {
				auth_profile = OS::LoadProfileFromJson(profile);
			}

			profile = json_object_get(send_json, "user");
			if (profile) {
				auth_user = OS::LoadUserFromJson(profile);
			}
			
		}
		request.authCallback(success, auth_user, auth_profile, auth_data, request.mp_extra, request.mp_peer);
		json_decref(send_json);
		return true;
	}
}
