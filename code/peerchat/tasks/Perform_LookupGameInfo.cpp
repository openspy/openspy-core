#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_LookupGameInfo(PeerchatBackendRequest request, TaskThreadData* thread_data) {
        OS::GameData game_info;
        if(!thread_data->shared_game_cache->LookupGameByName(request.gamename.c_str(), game_info)) {
            game_info = OS::GetGameByName(request.gamename.c_str(), thread_data->mp_redis_connection);
            thread_data->shared_game_cache->AddGame(thread_data->id, game_info);
        }


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