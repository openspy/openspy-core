#include "tasks.h"
#include <sstream>
#include <OS/HTTP.h>
#include <server/GSPeer.h>
namespace GS {
	bool Perform_NewGame(PersistBackendRequest request, TaskThreadData *thread_data) {
		json_t *send_json = json_object(), *profile_obj = json_object(), *game_obj = json_object();

		json_object_set_new(profile_obj, "id", json_integer(request.profileid));
		json_object_set_new(game_obj, "id", json_integer(request.mp_peer->GetGame().gameid));

		json_object_set_new(send_json, "profileLookup", profile_obj);
		json_object_set_new(send_json, "gameLookup", game_obj);

		std::string url = std::string(OS::g_webServicesURL);

		url += "/v1/Persist/Storage/NewGame";

		OS::HTTPClient client(url);
		PersistBackendResponse resp_data;

		char *json_data = json_dumps(send_json, 0);

		OS::HTTPResponse resp = client.Put(json_data, request.mp_peer);

		free(json_data);
		json_decref(send_json);

		send_json = json_loads(resp.buffer.c_str(), 0, NULL);

		bool success = false;

		json_t *success_obj = json_object_get(send_json, "_id");
		if (success_obj) {
			success = true;
			resp_data.game_instance_identifier = json_string_value(success_obj);
		}
		callback_dispatch_later(success, resp_data, request.mp_peer, request.mp_extra, request.callback);

		json_decref(send_json);
		request.mp_peer->DecRef();
		return false;
	}
}
