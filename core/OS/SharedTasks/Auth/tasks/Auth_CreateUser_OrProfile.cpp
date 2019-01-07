#include <OS/SharedTasks/tasks.h>
#include <OS/SharedTasks/Account/UserTasks.h>
#include <OS/SharedTasks/Account/ProfileTasks.h>
#include "../AuthTasks.h"
namespace TaskShared {
	//typedef void (*UserSearchCallback)(EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer);
	void RegisterUserCallback(EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer) {
			//try auth, try fetch profile, if no profile try create
			//if unique conflict, throw error
	}

	void UserLookupCallback(EUserResponseType response_type, std::vector<OS::User> results, void *extra, INetPeer *peer) {
		//if no user, register user, and create profile
		/* EProfileSearch_CreateProfile
		namespace TaskShared {
		bool PerformProfileRequest(ProfileRequest request, TaskThreadData *thread_data) {
		*/
		if (response_type == EUserResponseType_Success && results.size() > 0) {
			RegisterUserCallback(response_type, results, extra, peer);
		}
		else if(response_type == EUserResponseType_Success) {
			//register user
		}

	}
    bool PerformAuth_CreateUser_OrProfile(AuthRequest request, TaskThreadData *thread_data) {

        //try fetch user
        /*
        namespace TaskShared {
        bool   Perform_UserSearch(UserRequest request, TaskThreadData *thread_data) {
        */
		UserRequest userRequest;
		userRequest.type = TaskShared::EUserRequestType_Search;

		userRequest.search_params = request.user;
		userRequest.peer = request.peer;
		userRequest.peer->IncRef();
		userRequest.callback = UserLookupCallback;

		PerformUserRequest(userRequest, thread_data);


        return false;
    }
}