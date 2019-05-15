#include "tasks.h"
namespace Peerchat {
    bool Perform_ReserveNickname(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        TaskResponse response;
        response.error_details.response_code = TaskShared::WebErrorCode_Success;
        response.profile.uniquenick = request.profile.uniquenick;
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}