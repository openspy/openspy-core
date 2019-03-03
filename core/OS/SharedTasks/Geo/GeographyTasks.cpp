#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include <OS/SharedTasks/tasks.h>
#include "GeographyTasks.h"
namespace TaskShared {
	TaskScheduler<GeoRequest, TaskThreadData> *InitGeoTasks(INetServer *server) {
		TaskScheduler<GeoRequest, TaskThreadData> *scheduler = new TaskScheduler<GeoRequest, TaskThreadData>(OS::g_numAsync, server);
		scheduler->SetThreadDataFactory(TaskScheduler<GeoRequest, TaskThreadData>::DefaultThreadDataFactory);

		scheduler->AddRequestHandler(EGeoTaskType_GetCountries, PerformGeo_GetCountries);
		scheduler->DeclareReady();
		return scheduler;
	}
	void GeoReq_InitCurl(void *curl, char *post_data, void *write_data, GeoRequest request, struct curl_slist **out_list) {
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
		case EGeoTaskType_GetCountries:
			url += "/v1/Geography/countries";
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
			break;
		}

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

		/* set default user agent */
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "OSCDKey");

		/* set timeout */
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

		/* enable location redirects */
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

		/* set maximum allowed redirects */
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, write_data);

		/* Close socket after one use */
		curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);

		if(out_list) {
			*out_list = chunk;
		}
	}
}