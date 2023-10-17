#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>

#include <GP/server/GPPeer.h>
#include <GP/server/GPDriver.h>
#include <GP/server/GPServer.h>

namespace GP {
	void Peer::handle_removeblock(OS::KVReader data_parser) {
		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		if (data_parser.HasKey("profileid")) {
			int profileid = data_parser.GetValueInt("profileid");
			std::vector<int>::iterator it = std::find(m_blocks.begin(), m_blocks.end(), profileid);
			if (it == m_blocks.end()) {
				send_error(GPShared::GP_REMOVEBLOCK_NOT_BLOCKED);
				return;
			}
			m_blocks.erase(it);

			GPBackendRedisRequest req;
			req.type = EGPRedisRequestType_DelBlock;
			req.peer = this;
			req.peer->IncRef();
			req.ToFromData.to_profileid = profileid;
			req.ToFromData.from_profileid = m_profile.id;
			AddGPTaskRequest(req);
		}  else {
			send_error(GPShared::GP_PARSE);
			return;
		}
	}
}