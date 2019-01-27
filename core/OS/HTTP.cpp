#include <OS/HTTP.h>

namespace OS {
	struct curl_data {
	    std::string buffer;
	};
	/* callback for curl fetch */
	size_t HTTPClient::curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
		if(!contents) {
			return 0;
		}
	    size_t realsize = size * nmemb;                             /* calculate buffer size */
	    curl_data *data = (curl_data *)userp;
		const char *p = (const char *)contents;
		data->buffer += (p);

	    return realsize;
	}
	HTTPClient::~HTTPClient() {

	}
	HTTPClient::HTTPClient(std::string url) {
		m_url = url;
	}
	HTTPResponse HTTPClient::Post(std::string send, INetPeer *peer) {
		curl_data data;
		HTTPResponse resp;

		CURL *curl = curl_easy_init();
		CURLcode res;

		if(curl) {
			curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, send.c_str());

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OS/HTTP");

			/* set timeout */
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

			/* enable location redirects */
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

			/* set maximum allowed redirects */
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);

			/* Close socket after one use */
			curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &data);

			struct curl_slist *chunk = NULL;
			std::string apiKey = "APIKey: " + std::string(g_webServicesAPIKey);
			chunk = curl_slist_append(chunk, apiKey.c_str());
			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-App: ") + OS::g_appName).c_str());
			if (peer) {
				chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-Peer-Address: ") + peer->getAddress().ToString()).c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			res = curl_easy_perform(curl);

			if(res == CURLE_OK) {
				long http_code;
				curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);

				resp.status_code = http_code;
				resp.buffer = data.buffer;
				
			}
			else {
				resp.status_code = 0;
			}
			curl_easy_cleanup(curl);
		}

		return resp;
	}
}