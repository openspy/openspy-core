#ifndef _GP_TASKS_H
#define _GP_TASKS_H
#include <string>


#include <OS/GPShared.h>

#define GP_STATUS_EXPIRE_TIME 500

#include <OS/SharedTasks/Auth/AuthTasks.h>

#include <hiredis/hiredis.h>
#include <curl/curl.h>
#include <jansson.h>

#include <OS/SharedTasks/Account/UserTasks.h>
#include <OS/SharedTasks/Account/ProfileTasks.h>
#include <OS/SharedTasks/CDKey/CDKeyTasks.h>

#include <server/GPPeer.h>

using namespace TaskShared;

namespace GP {
    enum EGPRedisRequestType {
		EGPRedisRequestType_AddBuddy,
		EGPRedisRequestType_AuthorizeAdd,
		EGPRedisRequestType_UpdateStatus,
		EGPRedisRequestType_DelBuddy,
		//EGPRedisRequestType_RevokeAuth,
		EGPRedisRequestType_BuddyMessage,
		EGPRedisRequestType_AddBlock,
		EGPRedisRequestType_DelBlock,
		EGPRedisRequestType_SendGPBuddyStatus,
		EGPRedisRequestType_SendGPBlockStatus,
		EGPRedisRequestType_Auth_NickEmail_GPHash,
		EGPRedisRequestType_Auth_Uniquenick_GPHash,
		EGPRedisRequestType_Auth_PreAuth_Token_GPHash,
		EGPRedisRequestType_Auth_LoginTicket,
		EGPRedisRequestType_LookupBuddyStatus,
		EGPRedisRequestType_LookupBlockStatus
	};
    typedef struct {
		//std::vector<GP::Buddy
    } GPBackendRedisResponse;
	typedef void(*GPBackendRedisCallback)(bool success, GPBackendRedisResponse response_data, void *extra);
	typedef void(*BuddyStatusCallback)(TaskShared::WebErrorDetails error_details, std::map<int, GPShared::GPStatus> results, void *extra, INetPeer *peer);

	struct sBuddyRequest {
		int from_profileid;
		int to_profileid;
		std::string reason;
	};

	struct sToFrom {
		int from_profileid;
		int to_profileid;
	};
	

	struct sBuddyMessage {
		char type;
		int to_profileid;
		std::string message;
	};

	typedef struct {
		int type;
		//union {
			struct sBuddyRequest BuddyRequest;
			struct sBuddyMessage BuddyMessage;
			struct sToFrom ToFromData;
			
		//} uReqData;
		GPShared::GPStatus StatusInfo; //cannot be in union due to OS::Address
		void *extra;
		GP::Peer *peer;

		OS::User user;
		OS::Profile profile;

		TaskShared::AuthCallback authCallback;
		//gp hash specific
		std::string server_challenge;
		std::string client_challenge;
		std::string client_response;
		std::string auth_token;

		GPBackendRedisCallback callback;
		BuddyStatusCallback statusCallback;
		
	} GPBackendRedisRequest;

    bool Perform_BuddyRequest(GPBackendRedisRequest request, TaskThreadData *thread_data);
    bool Perform_SetPresenceStatus(GPBackendRedisRequest request, TaskThreadData *thread_data);
    bool Perform_SendLoginEvent(GPBackendRedisRequest request, TaskThreadData *thread_data);
    bool Perform_SendBuddyMessage(GPBackendRedisRequest request, TaskThreadData *thread_data);
	bool Perform_GetBuddyStatus(GPBackendRedisRequest request, TaskThreadData *thread_data);
	bool Perform_Auth_NickEmail_GPHash(GPBackendRedisRequest request, TaskThreadData *thread_data);
	bool Perform_Auth_Uniquenick_GPHash(GPBackendRedisRequest request, TaskThreadData *thread_data);
	bool Perform_Auth_PreAuth_Token_GPHash(GPBackendRedisRequest request, TaskThreadData *thread_data);
	bool Perform_Auth_LoginTicket_GPHash(GPBackendRedisRequest request, TaskThreadData *thread_data);
	bool Handle_PresenceMessage(TaskThreadData *thread_data, std::string message);
	bool Handle_AuthEvent(TaskThreadData *thread_data, std::string message);

	//authorizeadd, blockbuddy, addbuddy
	bool Perform_ToFromProfileAction(GPBackendRedisRequest request, TaskThreadData *thread_data);

    void InitTasks();
	void GPReq_InitCurl(void *curl, char *post_data, void *write_data, GPBackendRedisRequest request, struct curl_slist **out_list);

	void AddGPTaskRequest(GPBackendRedisRequest request);
}
#endif //_MM_TASKS_H