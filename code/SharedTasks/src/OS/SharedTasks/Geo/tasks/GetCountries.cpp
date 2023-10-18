
#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include "../GeographyTasks.h"

namespace TaskShared {
    bool PerformGeo_GetCountries(GeoRequest request, TaskThreadData *thread_data) {
		curl_data recv_data;


		CURL *curl = curl_easy_init();
		CURLcode res;
		OS::Profile profile;
		OS::User user;
		user.id = 0;
		profile.id = 0;
		TaskShared::GeoTaskData geo_data;
		
		if (curl) {
			struct curl_slist *chunk = NULL;
			GeoReq_InitCurl(curl, NULL, (void *)&recv_data, request, &chunk);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				json_t *json_data = json_loads(recv_data.buffer.c_str(), 0, NULL);
				if (Handle_WebError(json_data, geo_data.error_details)) {

				}
				else if (json_data && json_is_array(json_data)) {
					int num_items = json_array_size(json_data);
					for (int i = 0; i < num_items; i++) {
						json_t *arr_item = json_array_get(json_data, i);
						CountryRegion country;
						json_t *obj_item = json_object_get(arr_item, "countrycode");
						country.countrycode = json_string_value(obj_item);

						obj_item = json_object_get(arr_item, "countryname");
						country.countryname = json_string_value(obj_item);

						obj_item = json_object_get(arr_item, "region");
						country.region = json_integer_value(obj_item);
						geo_data.countries.push_back(country);
					}

					json_decref(json_data);
				}
			}
			curl_slist_free_all(chunk);
			curl_easy_cleanup(curl);
		}
		request.callback(geo_data, request.extra, request.peer);


		if (request.peer) {
			request.peer->DecRef();
		}
		return false;
    }
}