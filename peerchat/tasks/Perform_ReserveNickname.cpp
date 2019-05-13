#include "tasks.h"
namespace Peerchat {
    bool Perform_ReserveNickname(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        printf("RESERVE NICKNAME - %s\n", request.nick.c_str());

        TaskResponse response;
        response.error_details.response_code = TaskShared::WebErrorCode_Success;
        if(request.callback)
            request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
        return true;
    }
}