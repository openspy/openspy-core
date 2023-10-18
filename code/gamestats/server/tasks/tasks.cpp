#include "tasks.h"
#include <server/GSPeer.h>
namespace GS {
	void InitTasks() {
	}

	void PerformUVWorkRequest(uv_work_t *req) {
		TaskThreadData temp_data;
		PersistBackendRequest *request = (PersistBackendRequest *) uv_handle_get_data((uv_handle_t*) req);
		
		switch(request->type) {
			case EPersistRequestType_SetUserData:
				Perform_SetUserData(*request, &temp_data);
			break;
			case EPersistRequestType_GetUserData:
				Perform_GetUserData(*request, &temp_data);
			break;
			case EPersistRequestType_GetUserKeyedData:
				Perform_GetUserKeyedData(*request, &temp_data);
			break;
			case EPersistRequestType_NewGame:
				Perform_NewGame(*request, &temp_data);
			break;
			case EPersistRequestType_UpdateGame:
				Perform_UpdateGame(*request, &temp_data);
			break;
			case EPersistRequestType_GetGameInfoByGamename:
				Perform_GetGameInfo(*request, &temp_data);
			break;
			case EPersistRequestType_Auth_ProfileID:
				Perform_ProfileIDAuth(*request, &temp_data);
			break;
			case EPersistRequestType_Auth_AuthTicket:
				Perform_AuthTokenAuth(*request, &temp_data);
			break;
			case EPersistRequestType_Auth_CDKey:
				Perform_CDKeyAuth(*request, &temp_data);
			break;
			case EPersistRequestType_GetProfileIDFromCDKey:
				Perform_GetProfileIDFromCD(*request, &temp_data);
			break;
		}
	}

	void PerformUVWorkRequestCleanup(uv_work_t *req, int status) {
		PersistBackendRequest *work_data = (PersistBackendRequest *) uv_handle_get_data((uv_handle_t*) req);
		delete work_data;
		free((void *)req);
		
	}
}