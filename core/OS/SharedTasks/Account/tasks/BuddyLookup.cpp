#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>
#include "../ProfileTasks.h"
#include <sstream>
namespace TaskShared {
    bool Perform_BuddyRequest(ProfileRequest request, TaskThreadData *thread_data) {
		curl_data recv_data;
		json_t *root = NULL;
		std::map<int, OS::User> users;
		std::vector<OS::Profile> profiles;
		std::map<int, GPShared::GPStatus> status_map;
		//build json object
		json_t *send_obj = json_object(), *profile_obj = json_object();

		json_object_set_new(profile_obj, "id", json_integer(request.profile_search_details.id));
		json_object_set_new(send_obj, "profileLookup", profile_obj);

		if (request.type == EProfileSearch_Buddies_Reverse) {
			json_object_set_new(send_obj, "reverse", json_true());
		}


		char *json_data = json_dumps(send_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		TaskShared::WebErrorDetails error_details;

		if (curl) {
			ProfileReq_InitCurl(curl, json_data, (void *)&recv_data, request);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				root = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (root) {
					if (Handle_WebError(root, error_details)) {

					}
					else {
						int num_items = json_array_size(root);
						for (int i = 0; i < num_items; i++) {
							GPShared::GPStatus status;
							json_t *array_item = json_array_get(root, i);
							json_t *profile_item = json_object_get(array_item, "profile");
							json_t *user_item = json_object_get(array_item, "user");
							json_t *status_item = NULL;
							OS::Profile profile = OS::LoadProfileFromJson(profile_item);
							profiles.push_back(profile);
							users[profile.id] = OS::LoadUserFromJson(profile_item);

							status_item = json_object_get(array_item, "quietFlags");
							if(status_item && status_item != json_null())
								status.quiet_flags = json_integer_value(status_item);

							status_item = json_object_get(array_item, "statusFlags");
							if(status_item && status_item != json_null())
								status.status = (GPShared::GPEnum)json_integer_value(status_item);

							status_item = json_object_get(array_item, "statusText");
							if (status_item && status_item != json_null())
								status.status_str = json_string_value(status_item);

							if (status_item && status_item != json_null())
								status_item = json_object_get(array_item, "locationText");

							if(status_item && status_item != json_null())
								status.location_str = json_string_value(status_item);

							std::ostringstream address_ss;
							json_t *ip, *port;
							ip = json_object_get(array_item, "ip");
							port = json_object_get(array_item, "port");

							if ((ip && port) && (ip != json_null() && port != json_null())) {
								address_ss << json_string_value(ip) << ":";
								address_ss << json_integer_value(port);
								status.address = OS::Address(address_ss.str());
							}

							
							status_map[profile.id] = status;
						}
					}
				}
				else {
					error_details.response_code = TaskShared::WebErrorCode_BackendError;
				}
			}
			else {
				error_details.response_code = TaskShared::WebErrorCode_BackendError;
			}
			curl_easy_cleanup(curl);
		}

		if (root) {
			json_decref(root);
		}

		if (json_data)
			free((void *)json_data);

		if (send_obj)
			json_decref(send_obj);

		if (request.buddyCallback != NULL) {
			request.buddyCallback(error_details, profiles, users, status_map, request.extra, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
    }
}