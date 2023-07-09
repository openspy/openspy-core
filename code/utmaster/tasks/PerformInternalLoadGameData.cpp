#include <sstream>

#include "tasks.h"
#include <server/UTServer.h>

extern INetServer *g_gameserver;

namespace MM {
    bool PerformInternalLoadGameData(UTMasterRequest request, TaskThreadData *thread_data) {

        UT::Server *server = (UT::Server *)g_gameserver;
        
        server->doInternalLoadGameData(thread_data->mp_redis_connection);

        return true;
    }
}
