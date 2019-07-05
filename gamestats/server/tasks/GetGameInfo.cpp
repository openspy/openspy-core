#include <server/GSPeer.h>
#include "tasks.h"

namespace GS {
	bool Perform_GetGameInfo(PersistBackendRequest request, TaskThreadData *thread_data) {
		OS::GameData game_info;
		PersistBackendResponse resp_data;
        if(!thread_data->shared_game_cache->LookupGameByName(request.game_instance_identifier.c_str(), game_info)) {
            game_info = OS::GetGameByName(request.game_instance_identifier.c_str(), thread_data->mp_redis_connection);
            thread_data->shared_game_cache->AddGame(thread_data->id, game_info);
        }
		resp_data.gameData = game_info;
		request.callback(game_info.secretkey[0] != 0, resp_data, request.mp_peer, request.mp_extra);
		request.mp_peer->DecRef();
		return true;
	}
}
