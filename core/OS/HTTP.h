#ifndef _OS_HTTP_H
#define _OS_HTTP_H
#include <OS/OpenSpy.h>
#include <curl/curl.h>
namespace OS {
	typedef struct {
		int status_code;
		std::string buffer;
	} HTTPResponse;
	class HTTPClient {
	public:
		HTTPClient(std::string url);
		~HTTPClient();
		//(GP_PERSIST_BACKEND_URL, GP_PERSIST_BACKEND_CRYPTKEY, send_json)
		HTTPResponse Post(std::string send); //synchronous HTTP post

	private:
		static size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp);
		std::string m_url;
	};
}
#endif //_OS_HTTP_H