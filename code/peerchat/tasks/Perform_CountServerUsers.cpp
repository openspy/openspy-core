#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#include <OS/HTTP.h>

namespace Peerchat {

	bool Perform_CountServerUsers(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;
        response.usermode.usermodeid = CountServerUsers(thread_data);
        if (request.callback)
                request.callback(response, request.peer);

		if (request.peer)
			request.peer->DecRef();
		return true;
	}

}