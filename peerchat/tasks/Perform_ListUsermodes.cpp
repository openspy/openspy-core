#include "tasks.h"
#include <sstream>
#include <server/Server.h>

namespace Peerchat {
	bool Perform_ListUsermodes(PeerchatBackendRequest request, TaskThreadData* thread_data) {
		TaskResponse response;
        printf("perform list usermodes - %s\n", request.usermodeRecord.chanmask.c_str());
		UsermodeRecord usermode;
		//fake usermode 1
		usermode.chanmask = "X";
		usermode.usermodeid = 1;
		usermode.hostmask = "";
		usermode.profileid = 666;
		usermode.isGlobal = 1;
		usermode.modeflags = EUserChannelFlag_Gagged;
		usermode.setByUserSummary.nick = "SERVER";
		usermode.setByUserSummary.username = "SERVER";
		usermode.setByUserSummary.hostname = "*";
		usermode.setByUserSummary.profileid = 123456;
		usermode.expires_at.tv_sec = 0;
		usermode.set_at.tv_sec = 11111111;
		response.usermodes.push_back(usermode);
		//fake usermode 2
		usermode.chanmask = "#test";
		usermode.usermodeid = 2;
		usermode.hostmask = "127.0.0.1";
		usermode.profileid = 0;
		usermode.isGlobal = 1;
		usermode.modeflags = EUserChannelFlag_Banned;
		usermode.setByUserSummary.nick = "SERVER";
		usermode.setByUserSummary.username = "SERVER";
		usermode.setByUserSummary.hostname = "*";
		usermode.setByUserSummary.profileid = 123456;
		usermode.expires_at.tv_sec = 123123;
		usermode.set_at.tv_sec = 51111111;
		response.usermodes.push_back(usermode);


		//fake usermode 2
		usermode.chanmask = "#test2";
		usermode.usermodeid = 3;
		usermode.hostmask = "127.0.0.1";
		usermode.profileid = 0;
		usermode.isGlobal = 1;
		usermode.modeflags = EUserChannelFlag_Owner|EUserChannelFlag_Voice;
		usermode.setByUserSummary.nick = "SERVER";
		usermode.setByUserSummary.username = "SERVER";
		usermode.setByUserSummary.hostname = "*";
		usermode.setByUserSummary.profileid = 123456;
		usermode.expires_at.tv_sec = 123123;
		usermode.set_at.tv_sec = 51111111;
		response.usermodes.push_back(usermode);

		response.is_start = true;
		response.is_end = true;


		if (request.callback)
			request.callback(response, request.peer);
		if (request.peer)
			request.peer->DecRef();
		return true;
	}
}