#include <tasks/tasks.h>
#include <server/SBPeer.h>
namespace MM {
    bool PerformGetGameInfoPairByGameName(MMQueryRequest request, TaskThreadData *thread_data) {
		OS::GameData games[2];
        games[0] = OS::GetGameByName(request.gamenames[0].c_str(), thread_data->mp_redis_connection);
    
        games[1] = OS::GetGameByName(request.gamenames[1].c_str(), thread_data->mp_redis_connection);
		
		request.peer->OnRecievedGameInfoPair(games[0], games[1], request.extra);
        return true;
    }
}