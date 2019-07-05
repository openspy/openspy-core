#include <tasks/tasks.h>
#include <server/SBPeer.h>
namespace MM {
    bool PerformGetServerByKey(MMQueryRequest request, TaskThreadData *thread_data) {
		ServerListQuery ret;
		AppendServerEntry(thread_data, request.key, &ret, true, false, NULL);
		request.peer->OnRetrievedServerInfo(request, ret, request.extra);
		FreeServerListQuery(&ret);
		if(request.peer)
			request.peer->DecRef();
        return true;
    }
	Server *GetServerByKey(TaskThreadData *thread_data, std::string key, bool include_deleted) {
		ServerListQuery ret;
		AppendServerEntry(thread_data, key, &ret, true, include_deleted, NULL);
		if(ret.list.size() < 1)
			return NULL;

		return ret.list[0];
	}
}