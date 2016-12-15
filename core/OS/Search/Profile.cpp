#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>

#include <curl/curl.h>
#include <jansson.h>
#include <jwt.h>

namespace OS {
	struct curl_data {
	    json_t *json_data;
	};
	/* callback for curl fetch */
	size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
		if(!contents) {
			return 0;
		}
	    size_t realsize = size * nmemb;                             /* calculate buffer size */
	    curl_data *data = (curl_data *)userp;

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));

		jwt_decode(&jwt, (const char *)contents, NULL, 0);

		char *json = jwt_get_grants_json(jwt, NULL);
		printf("Json Data: %s\n", json);
		data->json_data = json_loads(json, 0, NULL);

		free(json);
		jwt_free(jwt);
	    return realsize;
	}
	void PerformSearch(ProfileSearchRequest request, ProfileSearchCallback cb, void *extra) {
		curl_data recv_data;
		std::vector<OS::Profile> results;
		std::map<int, OS::User> users_map;
		//build json object
		json_t *send_obj = json_object();

		json_object_set_new(send_obj, "mode", json_string("profile_search"));

		//user parameters
		if(request.email.length())
			json_object_set_new(send_obj, "email", json_string(request.email.c_str()));

		
		json_object_set_new(send_obj, "partnercode", json_integer(request.partnercode));

		//profile parameters
		if(request.nick.length())
			json_object_set_new(send_obj, "profilenick", json_string(request.nick.c_str()));

		if(request.uniquenick.length())
			json_object_set_new(send_obj, "uniquenick", json_string(request.uniquenick.c_str()));

		if(request.firstname.length())
			json_object_set_new(send_obj, "firstname", json_string(request.firstname.c_str()));

		if(request.lastname.length())
			json_object_set_new(send_obj, "lastname", json_string(request.lastname.c_str()));

		if(request.icquin)
			json_object_set_new(send_obj, "icquin", json_integer(request.icquin));

		if(request.namespaceids.size()) {
			json_t *namespaceids_json = json_array();

			//json_array_append_new(v_array, json_real(v));
			std::vector<int>::iterator it = request.namespaceids.begin();
			while(it != request.namespaceids.end()) {
				int v = *it;
				json_array_append_new(namespaceids_json, json_integer(v));
				it++;
			}

			json_object_set_new(send_obj, "namespaceids", namespaceids_json);

		}

		char *json_data = json_dumps(send_obj, 0);

		//build jwt
		jwt_t *jwt;
		jwt_new(&jwt);
		const char *server_response = NULL;
		jwt_set_alg(jwt, JWT_ALG_HS256, (const unsigned char *)OPENSPY_PROFILEMGR_KEY, strlen(OPENSPY_PROFILEMGR_KEY));
		jwt_add_grants_json(jwt, json_data);
		char *jwt_encoded = jwt_encode_str(jwt);

		CURL *curl = curl_easy_init();
		CURLcode res;

		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, OPENSPY_PROFILEMGR_URL);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jwt_encoded);

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSSearchProfile");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &recv_data);

			res = curl_easy_perform(curl);

			bool success = false;

			if(res == CURLE_OK) {
				json_t *profiles_obj = json_object_get(recv_data.json_data, "profiles");
				if(profiles_obj) {
					int num_profiles = json_array_size(profiles_obj);
					for(int i=0;i<num_profiles;i++) {

						json_t *profile_obj = json_array_get(profiles_obj, i);
						OS::Profile profile = OS::LoadProfileFromJson(profile_obj);
						if(users_map.find(profile.userid) == users_map.end()) {
							json_t *user_obj = json_object_get(profile_obj, "user");
							users_map[profile.userid] = OS::LoadUserFromJson(user_obj);
						}
						results.push_back(profile);
					}
				}
			}
		}
		cb(res == CURLE_OK, results, users_map, extra);
	}
}