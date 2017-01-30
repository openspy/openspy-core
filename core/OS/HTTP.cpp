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
		while(*p) {
			if(isalnum(*p) || *p == '.')
				data->buffer += *(p);
			else break;
			p++;
		}

	    return realsize;
	}
	HTTPClient::~HTTPClient() {

	}
	HTTPClient::HTTPClient(std::string url) {
		m_url = url;
	}
	HTTPResponse HTTPClient::Post(std::string send) {
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

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &data);

			res = curl_easy_perform(curl);

			if(res == CURLE_OK) {
				long http_code;
				curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);

				resp.status_code = http_code;
				resp.buffer = data.buffer;
				
			}
			curl_easy_cleanup(curl);
		}

		return resp;
	}
}