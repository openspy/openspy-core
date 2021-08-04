#include <OS/HTTP.h>

namespace OS {
	struct curl_data {
	    std::string buffer;
	};
	/* callback for curl fetch */
	size_t HTTPClient::curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
		if (!contents) {
			return 0;
		}
		size_t realsize = size * nmemb;                             /* calculate buffer size */
		curl_data *data = (curl_data *)userp;
		std::string buffer = std::string((const char *)contents, realsize);
		data->buffer += OS::strip_whitespace(buffer.c_str()).c_str();
		return realsize;
	}
	HTTPClient::~HTTPClient() {

	}
	HTTPClient::HTTPClient(std::string url) {
		m_url = url;
	}
	HTTPResponse HTTPClient::Post(std::string send, INetPeer *peer) {
		return PerformMethod(send, "", peer);
	}

	HTTPResponse HTTPClient::Put(std::string send, INetPeer* peer) {
		return PerformMethod(send, "PUT", peer);
	}
	HTTPResponse HTTPClient::Delete(std::string send, INetPeer* peer) {
		return PerformMethod(send, "DELETE", peer);
	}
	HTTPResponse HTTPClient::PerformMethod(std::string send, std::string method, INetPeer* peer) {
		curl_data data;
		HTTPResponse resp;

		CURL* curl = curl_easy_init();
		CURLcode res;

		if (curl) {
			curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
			curl_easy_setopt(curl, CURLOPT_SHARE, OS::g_curlShare);

			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, send.c_str());

			/* set default user agent */
			curl_easy_setopt(curl, CURLOPT_USERAGENT, "OS/HTTP");

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
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);


			if(method.length() > 0)
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

			struct curl_slist* chunk = NULL;
			std::string apiKey = "APIKey: " + std::string(g_webServicesAPIKey);
			chunk = curl_slist_append(chunk, apiKey.c_str());
			chunk = curl_slist_append(chunk, "Content-Type: application/json");
			chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-App: ") + OS::g_appName).c_str());
			if (peer) {
				chunk = curl_slist_append(chunk, std::string(std::string("X-OpenSpy-Peer-Address: ") + peer->getAddress().ToString()).c_str());
			}
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {
				long http_code;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

				resp.status_code = http_code;
				resp.buffer = data.buffer;

			}
			else {
				resp.status_code = 0;
			}
			curl_slist_free_all(chunk);
			curl_easy_cleanup(curl);
		}

		return resp;
	}
}