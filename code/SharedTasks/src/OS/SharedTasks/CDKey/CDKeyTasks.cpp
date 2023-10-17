#include <OS/OpenSpy.h>
#include <OS/Net/NetPeer.h>
#include "CDKeyTasks.h"
namespace TaskShared {
    // TaskScheduler<CDKeyRequest, TaskThreadData>::RequestHandlerEntry CDKey_RequestHandler[] = {
    //     {ECDKeyType_AssociateToProfile, PerformCDKey_AssociateToProfile},
    //     {ECDKeyType_GetProfileByCDKey, PerformCDKey_GetProfileByCDKey},
    //     {ECDKeyType_TestCDKeyValid, PerformCDKey_TestCDKeyValid},
    //     {-1, NULL}
    // };
    // TaskScheduler<CDKeyRequest, TaskThreadData> *InitCDKeyTasks(INetServer *server) {
    //         TaskScheduler<CDKeyRequest, TaskThreadData> *scheduler = new TaskScheduler<CDKeyRequest, TaskThreadData>(OS::g_numAsync, server, CDKey_RequestHandler, NULL);
    //         scheduler->SetThreadDataFactory(TaskScheduler<CDKeyRequest, TaskThreadData>::DefaultThreadDataFactory);
	// 		scheduler->DeclareReady();
    //         return scheduler;
    // }
    void CDKeyReq_InitCurl(void *curl, char *post_data, void *write_data, CDKeyRequest request, struct curl_slist **out_list) {
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
            case ECDKeyType_AssociateToProfile:
                url += "/v1/CDKey/AssociateCDKeyToProfile";
                break;
            case ECDKeyType_GetProfileByCDKey:
                url += "/v1/CDKey/GetProfileByCDKey";
                break;
            case ECDKeyType_TestCDKeyValid:
                url += "/v1/CDKey/TestCDKeyValid";
                break;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_SHARE, OS::g_curlShare);

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
}