#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>
#include "ProfileTasks.h"
#include "UserTasks.h"
#define MAX_SUGGESTIONS 5
namespace TaskShared {

		TaskScheduler<UserRequest, TaskThreadData>::RequestHandlerEntry UserTasks_requestTable[] = {
			{EUserRequestType_Search, PerformUserRequest},
			{EUserRequestType_Update, PerformUserRequest},
			{EUserRequestType_Create, PerformUserRegisterRequest},
			{NULL, NULL}
		};
        TaskScheduler<UserRequest, TaskThreadData> *InitUserTasks(INetServer *server) {
            TaskScheduler<UserRequest, TaskThreadData> *scheduler = new TaskScheduler<UserRequest, TaskThreadData>(OS::g_numAsync, server, UserTasks_requestTable, NULL);
            scheduler->SetThreadDataFactory(TaskScheduler<UserRequest, TaskThreadData>::DefaultThreadDataFactory);
			scheduler->DeclareReady();
            return scheduler;
        }
		TaskScheduler<ProfileRequest, TaskThreadData>::RequestHandlerEntry ProfileTasks_requestTable[] = {
			{EProfileSearch_Profiles, PerformProfileRequest},
			{EProfileSearch_CreateProfile, PerformProfileRequest},
			{EProfileSearch_DeleteProfile, PerformProfileRequest},
			{EProfileSearch_UpdateProfile, PerformProfileRequest},
			{EProfileSearch_SuggestUniquenick, Perform_SuggestUniquenick},

			{EProfileSearch_Buddies, Perform_BuddyRequest},
			{EProfileSearch_Buddies_Reverse, Perform_BuddyRequest},
			{EProfileSearch_Blocks, Perform_BuddyRequest},
			{NULL, NULL}
		};
        TaskScheduler<ProfileRequest, TaskThreadData> *InitProfileTasks(INetServer *server) {
            TaskScheduler<ProfileRequest, TaskThreadData> *scheduler = new TaskScheduler<ProfileRequest, TaskThreadData>(OS::g_numAsync, server, ProfileTasks_requestTable, NULL);

			scheduler->DeclareReady();
            return scheduler;
        }
		void ProfileReq_InitCurl(void *curl, char *post_data, void *write_data, ProfileRequest request, struct curl_slist **out_list) {
			struct curl_slist *chunk = NULL;
			std::string apiKey = "APIKey: " + std::string(OS::g_webServicesAPIKey);
			chunk = curl_slist_append(chunk, apiKey.c_str());
			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-App: ") + OS::g_appName).c_str());
			if (request.peer) {
				chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-Peer-Address: ") + request.peer->getAddress().ToString()).c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			std::string url = OS::g_webServicesURL;
			switch (request.type) {
			case EProfileSearch_Profiles:
				url += "/v1/Profile/lookup";
				break;
			case EProfileSearch_SuggestUniquenick:
				url += "/v1/Profile/GetNameSuggestions";
				break;
			case EProfileSearch_CreateProfile:
			case EProfileSearch_DeleteProfile:
			case EProfileSearch_UpdateProfile:
				url += "/v1/Profile";
				break;
			case EProfileSearch_Buddies:
			case EProfileSearch_Buddies_Reverse:
				url += "/v1/Presence/Status/FindBuddyStatuses";
				break;
			case EProfileSearch_Blocks:
				url += "/v1/Presence/Status/FindBlockStatuses";
				break;
			}
			switch (request.type) {
			case EProfileSearch_CreateProfile:
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
				break;
			case EProfileSearch_DeleteProfile:
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
				break;
			}
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_SHARE, OS::g_curlShare);

			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSSearchProfile");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_data);

			/* enable TCP keep-alive for this transfer */
			curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
			
			/* set keep-alive idle time to 120 seconds */
			curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
			
			/* interval time between keep-alive probes: 60 seconds */
			curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

			if(out_list != NULL) {
				*out_list = chunk;
			}
		}
		bool PerformProfileRequest(ProfileRequest request, TaskThreadData *thread_data) {
			curl_data recv_data;
			recv_data.buffer = "";
			std::vector<OS::Profile> results;
			std::map<int, OS::User> users_map;
			//build json object

			json_t *profile_obj = json_object();
			json_t *send_obj = profile_obj;
			json_t *user_obj = json_object();
			if (request.profile_search_details.id != 0)
				json_object_set_new(profile_obj, "id", json_integer(request.profile_search_details.id));

			if (request.profile_search_details.userid != 0)
			json_object_set_new(profile_obj, "userid", json_integer(request.profile_search_details.userid));

			//user parameters
			if (request.user_search_details.email.length()) {
				json_object_set_new(user_obj, "email", json_string(request.user_search_details.email.c_str()));
			}

			if (request.user_search_details.id != 0) {
				json_object_set_new(profile_obj, "userid", json_integer(request.user_search_details.id));
				json_object_set_new(user_obj, "id", json_integer(request.user_search_details.id));

			}


			if (request.user_search_details.partnercode != -1) {
				json_object_set_new(user_obj, "partnercode", json_integer(request.user_search_details.partnercode));
			}



			//profile parameters
			if (request.profile_search_details.nick.length())
				json_object_set_new(profile_obj, "nick", json_string(request.profile_search_details.nick.c_str()));

			if (request.profile_search_details.uniquenick.length())
				json_object_set_new(profile_obj, "uniquenick", json_string(request.profile_search_details.uniquenick.c_str()));

			if (request.profile_search_details.firstname.length())
				json_object_set_new(profile_obj, "firstname", json_string(request.profile_search_details.firstname.c_str()));

			if (request.profile_search_details.lastname.length())
				json_object_set_new(profile_obj, "lastname", json_string(request.profile_search_details.lastname.c_str()));

			if (request.profile_search_details.icquin)
				json_object_set_new(profile_obj, "icquin", json_integer(request.profile_search_details.icquin));

			if (request.profile_search_details.zipcode)
				json_object_set_new(profile_obj, "zipcode", json_integer(request.profile_search_details.zipcode));


			if (request.profile_search_details.sex != -1)
				json_object_set_new(profile_obj, "sex", json_integer(request.profile_search_details.sex));

			if (request.profile_search_details.pic != 0)
				json_object_set_new(profile_obj, "pic", json_integer(request.profile_search_details.pic));
			if (request.profile_search_details.ooc != 0)
				json_object_set_new(profile_obj, "ooc", json_integer(request.profile_search_details.ooc));
			if (request.profile_search_details.ind != 0)
				json_object_set_new(profile_obj, "ind", json_integer(request.profile_search_details.ind));
			if (request.profile_search_details.mar != 0)
				json_object_set_new(profile_obj, "mar", json_integer(request.profile_search_details.mar));
			if (request.profile_search_details.chc != 0)
				json_object_set_new(profile_obj, "chc", json_integer(request.profile_search_details.chc));
			if (request.profile_search_details.i1 != 0)
				json_object_set_new(profile_obj, "i1", json_integer(request.profile_search_details.i1));

			if (request.profile_search_details.birthday.GetYear() != 0)
				json_object_set_new(profile_obj, "birthday", request.profile_search_details.birthday.GetJson());

			if (request.profile_search_details.lon)
				json_object_set_new(profile_obj, "lon", json_real(request.profile_search_details.lon));
			if (request.profile_search_details.lat)
				json_object_set_new(profile_obj, "lat", json_real(request.profile_search_details.lat));

			if (request.profile_search_details.countrycode.length() > 0)
				json_object_set_new(profile_obj, "countrycode", json_string(request.profile_search_details.countrycode.c_str()));


			if (request.profile_search_details.namespaceid != -1)
				json_object_set_new(profile_obj, "namespaceid", json_integer(request.profile_search_details.namespaceid));


			if (request.namespaceids.size()) {
				json_t *namespaceids_json = json_array();

				//json_array_append_new(v_array, json_real(v));
				std::vector<int>::iterator it = request.namespaceids.begin();
				while (it != request.namespaceids.end()) {
					int v = *it;
					json_array_append_new(namespaceids_json, json_integer(v));
					it++;
				}

				json_object_set_new(profile_obj, "namespaceids", namespaceids_json);

			}

			if (request.target_profileids.size()) {
				json_t *profileids_json = json_array();

				std::vector<int>::iterator it = request.target_profileids.begin();
				while (it != request.target_profileids.end()) {
					int v = *it;
					json_array_append_new(profileids_json, json_integer(v));
					it++;
				}

				json_object_set_new(profile_obj, "target_profileids", profileids_json);
			}
			//json_object_set(send_obj, "profile", profile_obj);

			if(json_object_size(user_obj) > 0)
				json_object_set_new(send_obj, "user", user_obj);


			char *json_string = json_dumps(send_obj, 0);

			CURL *curl = curl_easy_init();
			CURLcode res;
			struct curl_slist *chunk = NULL;
			TaskShared::WebErrorDetails error_details;
			if (curl) {

				ProfileReq_InitCurl(curl, json_string, (void *)&recv_data, request, &chunk);
				res = curl_easy_perform(curl);
				if (res == CURLE_OK) {
					json_t *json_data = NULL;

					json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
					if (Handle_WebError(json_data, error_details)) {

					}
					else if (json_data) {
						if (json_is_array(json_data)) {
							size_t num_profiles = json_array_size(json_data);
							for (size_t i = 0; i < num_profiles; i++) {
								json_t *profile_obj = json_array_get(json_data, i);
								OS::Profile profile = OS::LoadProfileFromJson(profile_obj);
								if (users_map.find(profile.userid) == users_map.end()) {
									json_t *user_obj = json_object_get(profile_obj, "user");
									users_map[profile.userid] = OS::LoadUserFromJson(user_obj);
								}
								results.push_back(profile);
							}
						}
						else if (json_is_true(json_data)) {
							//success
						}
						else {
							//check for single profile
							OS::Profile profile = OS::LoadProfileFromJson(json_data);
							if(profile.id > 0)
								results.push_back(profile);
						}
						json_decref(json_data);
					}
					else {
						error_details.response_code = TaskShared::WebErrorCode_BackendError;
					}
				}
				curl_slist_free_all(chunk);
				curl_easy_cleanup(curl);
			}

			if (json_string) {
				free((void *)json_string);
			}
			if (send_obj)
				json_decref(send_obj);

			request.callback(error_details, results, users_map, request.extra, request.peer);

			if (request.peer) {
				request.peer->DecRef();
			}
			return true;
		}
		bool PerformUserRequest(UserRequest request, TaskThreadData *thread_data) {
			curl_data recv_data;
			json_t *root = NULL;
			std::vector<OS::User> results;
			//build json object
			json_t *user_obj = OS::UserToJson(request.search_params);

			char *json_data = json_dumps(user_obj, 0);

			CURL *curl = curl_easy_init();
			CURLcode res;
			TaskShared::WebErrorDetails error_details;

			if (curl) {
				std::string url = std::string(OS::g_webServicesURL);
				switch (request.type) {
				case EUserRequestType_Search:
					url += "/v1/User/lookup";
					break;
				case EUserRequestType_Update:
					url += "/v1/User";
					break;
				case EUserRequestType_Register:
					url += "/v1/User/register";
					break;
				break;
				}

				curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
				curl_easy_setopt(curl, CURLOPT_SHARE, OS::g_curlShare);

				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

				/* set default user agent */
				curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSSearchUser");

				/* set timeout */
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

				/* enable location redirects */
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

				/* set maximum allowed redirects */
				curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

				/* enable TCP keep-alive for this transfer */
				curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
				
				/* set keep-alive idle time to 120 seconds */
				curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
				
				/* interval time between keep-alive probes: 60 seconds */
				curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

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
						if (Handle_WebError(root, error_details)) {

						} else if (json_array_size(root) > 0) {
							OS::User user = OS::LoadUserFromJson(json_array_get(root, 0));
							results.push_back(user);
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

			if (user_obj)
				json_decref(user_obj);


			if (request.callback != NULL)
				request.callback(error_details, results, request.extra, request.peer);

			if (request.peer) {
				request.peer->DecRef();
			}
			return true;
		}
		bool Perform_SuggestUniquenick(ProfileRequest request, TaskThreadData *thread_data) {
			curl_data recv_data;
			recv_data.buffer = "";
			std::vector<OS::Profile> results;
			std::map<int, OS::User> users_map;
			//build json object

			json_t *send_obj = json_object();

			//profile parameters

			if (request.profile_search_details.uniquenick.length())
				json_object_set_new(send_obj, "preferredName", json_string(request.profile_search_details.uniquenick.c_str()));

			if (request.profile_search_details.namespaceid != -1)
				json_object_set_new(send_obj, "namespaceid", json_integer(request.profile_search_details.namespaceid));

			json_object_set_new(send_obj, "num_suggestions", json_integer(MAX_SUGGESTIONS));



			char *json_string = json_dumps(send_obj, 0);

			CURL *curl = curl_easy_init();
			CURLcode res;
			struct curl_slist *chunk = NULL;
			TaskShared::WebErrorDetails error_details;
			if (curl) {

				ProfileReq_InitCurl(curl, json_string, (void *)&recv_data, request, &chunk);
				res = curl_easy_perform(curl);
				if (res == CURLE_OK) {
					json_t *json_data = NULL;

					json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
					if (Handle_WebError(json_data, error_details)) {

					}
					else if (json_data && json_is_array(json_data)) {
						int arr_len = json_array_size(json_data);
						for(int i=0;i<arr_len;i++) {
							OS::Profile suggestedProfile;
							suggestedProfile.uniquenick = json_string_value(json_array_get(json_data, i));
							results.push_back(suggestedProfile);
						}
					}
					else {
						error_details.response_code = TaskShared::WebErrorCode_BackendError;
					}
				}
				curl_slist_free_all(chunk);
				curl_easy_cleanup(curl);
			}

			if (json_string) {
				free((void *)json_string);
			}
			if (send_obj)
				json_decref(send_obj);

			request.callback(error_details, results, users_map, request.extra, request.peer);

			if (request.peer) {
				request.peer->DecRef();
			}
			return true;
		}
}