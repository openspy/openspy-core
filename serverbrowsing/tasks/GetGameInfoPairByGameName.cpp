#include <tasks/tasks.h>
#include <server/SBPeer.h>
namespace MM {
    bool PerformGetGameInfoPairByGameName(MMQueryRequest request, TaskThreadData *thread_data) {
		OS::GameData games[2];
        if(!thread_data->shared_game_cache->LookupGameByName(request.gamenames[0].c_str(), games[0])) {
            games[0] = OS::GetGameByName(request.gamenames[0].c_str(), thread_data->mp_redis_connection);
            thread_data->shared_game_cache->AddGame(thread_data->id, games[0]);
        }
        if(!thread_data->shared_game_cache->LookupGameByName(request.gamenames[1].c_str(), games[1])) {
            games[1] = OS::GetGameByName(request.gamenames[1].c_str(), thread_data->mp_redis_connection);
            thread_data->shared_game_cache->AddGame(thread_data->id, games[1]);
        }
		
		request.peer->OnRecievedGameInfoPair(games[0], games[1], request.extra);
        return true;
    }
}