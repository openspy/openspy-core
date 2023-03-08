#include <tasks/tasks.h>
#include <serverbrowsing/filter/filter.h>
#include <server/SBPeer.h>
#include <algorithm>
#include <sstream>

namespace MM {

	void AppendServerEntry_AllKeys(TaskThreadData* thread_data, std::string server_key, ServerListQuery* ret, bool include_deleted, bool all_player_and_team_keys) {
		sServerListReq req;
		req.all_keys = true;
		req.include_deleted = include_deleted;

		if (all_player_and_team_keys) {
			req.all_player_keys = true;
			req.all_team_keys = true;
		}

		std::vector<std::string> keys;
		keys.push_back(server_key);

		AppendServerEntries_AllKeys(thread_data, keys, ret, &req);
	}

    bool PerformGetServerByKey(MMQueryRequest request, TaskThreadData *thread_data) {
		ServerListQuery ret;
		AppendServerEntry_AllKeys(thread_data, request.key, &ret, false, true);
		request.peer->OnRetrievedServerInfo(request, ret, request.extra);
		FreeServerListQuery(&ret);
		if(request.peer)
			request.peer->DecRef();
        return true;
    }
	Server *GetServerByKey(TaskThreadData *thread_data, std::string key, bool include_deleted, bool include_player_and_team_keys) {
		ServerListQuery ret;
		AppendServerEntry_AllKeys(thread_data, key, &ret, include_deleted, include_player_and_team_keys);
		if(ret.list.size() < 1)
			return NULL;

		return ret.list[0];
	}
}