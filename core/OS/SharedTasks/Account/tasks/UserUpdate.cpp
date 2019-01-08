#include <OS/SharedTasks/tasks.h>
#include "../UserTasks.h"

namespace TaskShared {
    bool Perform_UserUpdate(UserRequest request, TaskThreadData *thread_data) {
		curl_data recv_data;
		json_t *root = NULL;
		std::vector<OS::User> results;
		//build json object
		json_t *user_obj = json_object();

		if (request.search_params.id != 0) {
			json_object_set_new(user_obj, "id", json_integer(request.search_params.id));
		}

		//user parameters
		if (request.search_params.email.length())
			json_object_set_new(user_obj, "email", json_string(request.search_params.email.c_str()));


		if (request.type == EUserRequestType_Update) {
			if (request.search_params.password.length())
				json_object_set_new(user_obj, "password", json_string(request.search_params.password.c_str()));

			json_object_set_new(user_obj, "videocard1ram", json_integer(request.search_params.videocard_ram[0]));
			json_object_set_new(user_obj, "videocard2ram", json_integer(request.search_params.videocard_ram[1]));

			json_object_set_new(user_obj, "cpuspeed", json_integer(request.search_params.cpu_speed));
			json_object_set_new(user_obj, "cpubrandid", json_integer(request.search_params.cpu_brandid));
			json_object_set_new(user_obj, "connectionspeed", json_integer(request.search_params.connectionspeed));
			json_object_set_new(user_obj, "hasnetwork", request.search_params.hasnetwork ? json_true() : json_false());
			json_object_set_new(user_obj, "publicmask", json_integer(request.search_params.publicmask));
		}
		if (request.search_params.partnercode != -1) {
			json_object_set_new(user_obj, "partnercode", json_integer(request.search_params.partnercode));
		}

		char *json_data = json_dumps(user_obj, 0);

		CURL *curl = curl_easy_init();
		CURLcode res;
		EUserResponseType resp_type = EUserResponseType_Success;

		if (curl) {
			std::string url = std::string(OS::g_webServicesURL) + "/v1/User";

			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSSearchUser");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			/* Close socket after one use */
			curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&recv_data);

			struct curl_slist *chunk = NULL;
			std::string apiKey = "APIKey: " + std::string(OS::g_webServicesAPIKey);
			chunk = curl_slist_append(chunk, apiKey.c_str());
			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-App: ") + OS::g_appName).c_str());
			if (request.peer) {
				chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-Peer-Address: ") + request.peer->getAddress().ToString()).c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				root = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (root) {
					if (json_array_size(root) > 0) {
						OS::User user = OS::LoadUserFromJson(json_array_get(root, 0));
						results.push_back(user);
						resp_type = EUserResponseType_Success;
					}
				}
				else {
					resp_type = EUserResponseType_GenericError;
				}
			}
			else {
				resp_type = EUserResponseType_GenericError;
			}
			curl_easy_cleanup(curl);
		}

		if (root) {
			json_decref(root);
		}

		if (json_data)
			free((void *)json_data);

		if (user_obj)
			json_decref(user_obj);


		if (request.callback != NULL)
			request.callback(resp_type, results, request.extra, request.peer);

		if (request.peer) {
			request.peer->DecRef();
		}
		return true;
	}
}