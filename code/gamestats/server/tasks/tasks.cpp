#include "tasks.h"
#include <server/GSPeer.h>
namespace GS {
		TaskScheduler<PersistBackendRequest, TaskThreadData>::RequestHandlerEntry requestTable[] = {
			{EPersistRequestType_SetUserData, Perform_SetUserData},
			{EPersistRequestType_GetUserData, Perform_GetUserData},
			{EPersistRequestType_GetUserKeyedData, Perform_GetUserKeyedData},
			{EPersistRequestType_NewGame, Perform_NewGame},
			{EPersistRequestType_UpdateGame, Perform_UpdateGame},
			{EPersistRequestType_GetGameInfoByGamename, Perform_GetGameInfo},
			{EPersistRequestType_Auth_ProfileID, Perform_ProfileIDAuth},
			{EPersistRequestType_Auth_AuthTicket, Perform_AuthTokenAuth},
			{EPersistRequestType_Auth_CDKey, Perform_CDKeyAuth},
			{EPersistRequestType_GetProfileIDFromCDKey, Perform_GetProfileIDFromCD},
			{-1, NULL}
		};
        TaskScheduler<PersistBackendRequest, TaskThreadData> *InitTasks(INetServer *server) {
            TaskScheduler<PersistBackendRequest, TaskThreadData> *scheduler = new TaskScheduler<PersistBackendRequest, TaskThreadData>(OS::g_numAsync, server, requestTable, NULL);

			scheduler->DeclareReady();

            return scheduler;
        }
}