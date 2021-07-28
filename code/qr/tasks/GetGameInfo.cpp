
#include <tasks/tasks.h>
namespace MM {
    bool PerformGetGameInfo(MMPushRequest request, TaskThreadData  *thread_data) {
		OS::GameData game_info;
        game_info = OS::GetGameByName(request.gamename.c_str(), thread_data->mp_redis_connection);

        MMTaskResponse response;
        response.driver = request.driver;
        response.from_address = request.from_address;
        response.v2_instance_key = request.v2_instance_key;

        request.callback(response);

        return true;
    }
}