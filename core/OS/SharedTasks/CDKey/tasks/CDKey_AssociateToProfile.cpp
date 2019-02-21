#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>

namespace TaskShared {
    bool PerformCDKey_AssociateToProfile(CDKeyRequest request, TaskThreadData *thread_data) {
		curl_data recv_data;
		//build json object
		json_t *send_obj = json_object(), *profile_obj = json_object(), *game_obj = json_object();

        profile_obj = OS::ProfileToJson(request.profile);
        json_object_set_new(send_obj, "profileLookup", profile_obj);

        if(request.gameid != 0) {
            json_object_set_new(game_obj, "id", json_integer(request.gameid));
        }
        if(request.gamename.length() != 0) {
            json_object_set_new(game_obj, "gamename", json_string(request.gamename.c_str()));
        }
            
        json_object_set_new(send_obj, "gameLookup", game_obj);

        json_object_set_new(send_obj, "cdkey", json_string(request.cdkey.c_str()));

        
		char *json_dump = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		OS::Profile profile;
		OS::User user;
		user.id = 0;
		profile.id = 0;
		TaskShared::CDKeyData cdkey_data;

		bool success = false;
		if (curl) {

			CDKeyReq_InitCurl(curl, json_dump, (void *)&recv_data, request);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (Handle_WebError(json_data, cdkey_data.error_details)) {

				}
				else if (json_data) {
					success = true;

					json_decref(json_data);
				}
			}
			curl_easy_cleanup(curl);
		}
		request.callback(cdkey_data, request.extra, request.peer);
		if (json_dump) {
			free((void *)json_dump);
		}
		json_decref(send_obj);

		if (request.peer) {
			request.peer->DecRef();
		}
		return false;
    }
}