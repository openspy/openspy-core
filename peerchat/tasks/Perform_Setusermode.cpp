#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_SetUsermode(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;
        printf("perform set usermode - %s\n", request.usermodeRecord.chanmask.c_str());
		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}