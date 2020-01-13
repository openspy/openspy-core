#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_DeleteUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;
        printf("perform delete usermode - %d\n", request.usermodeRecord.usermodeid);
		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}