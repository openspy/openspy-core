#include "tasks.h"
#include <sstream>
#include <server/Server.h>
#include <OS/HTTP.h>
namespace Peerchat {

	bool Perform_RemoteKill_ByName(PeerchatBackendRequest request, TaskThreadData* thread_data) { 
		TaskResponse response;
        
		UserSummary summary = GetUserSummaryByName(thread_data, request.summary.username);
        response.summary = summary;

        const char* base64 = OS::BinToBase64Str((uint8_t*)request.usermodeRecord.comment.c_str(), request.usermodeRecord.comment.length());

        std::ostringstream message;

		message << "\\type\\REMOTE_KILL\\user_id\\" << summary.id << "\\reason\\" << base64;

        free((void*)base64);
		thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_key_updates_routingkey, message.str().c_str());		

        response.profile.uniquenick = request.summary.username;
		
		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}