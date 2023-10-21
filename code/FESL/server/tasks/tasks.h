#ifndef _FESL_TASKS_H
#define _FESL_TASKS_H
#include <string>

#include <OS/SharedTasks/Auth/AuthTasks.h>

#include <curl/curl.h>
#include <jansson.h>

#include <OS/SharedTasks/Account/ProfileTasks.h>
#include <OS/SharedTasks/Geo/GeographyTasks.h>

#include <server/FESLServer.h>
#include <server/FESLDriver.h>
#include <server/FESLPeer.h>

#include <OS/tasks.h>

using namespace TaskShared;

namespace FESL {   
     enum EFESLRequestType {
        EFESLRequestType_GetEntitledGameFeatures,
        EFESLRequestType_GetObjectInventory,
        EFESLRequestType_GetEntitlementBundle,
    };

    class EntitledGameFeature {
        public:
            int GameFeatureId;
            int Status;
            time_t EntitlementExpirationDate;
            std::string Message;
            int32_t EntitlementExpirationDays;
    };


    class ObjectInventoryItem {
        public:
            std::string ObjectId;
            int EditionNo;
            time_t DateEntitled;
            uint32_t UseCount;
            uint32_t EntitleId;
    };


    typedef void (*EntitledGameFeaturesLookupCallback)(TaskShared::WebErrorDetails error_details, std::vector<EntitledGameFeature> results, INetPeer *peer, void *extra);
    typedef void (*ObjectInventoryItemsLookupCallback)(TaskShared::WebErrorDetails error_details, std::vector<ObjectInventoryItem> results, INetPeer *peer, void *extra);

	class FESLRequest {
        public:
            int type;
            int profileid;
            FESL::Peer *peer;

            PublicInfo driverInfo;

            std::vector<std::string> objectIds;
            void *extra;

            EntitledGameFeaturesLookupCallback gameFeaturesCallback;
            ObjectInventoryItemsLookupCallback objectInventoryItemsCallback;
	};


    bool Perform_GetEntitledGameFeatures(FESLRequest request, TaskThreadData *thread_data);
    bool Perform_GetObjectInventory(FESLRequest request, TaskThreadData *thread_data);

    bool Handle_AuthEvent(TaskThreadData *thread_data, std::string message);
    void InitTasks();
    void AddFESLTaskRequest(FESLRequest request);
}
#endif //_MM_TASKS_H