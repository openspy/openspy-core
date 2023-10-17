#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_LookupGameInfo(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        OS::GameData game_info;
        game_info = OS::GetGameByName(request.gamename.c_str(), thread_data->mp_redis_connection);


        TaskResponse response;
        response.game_data = game_info;

        if (request.callback) {
			request.callback(response, request.peer);
		}
		if (request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}