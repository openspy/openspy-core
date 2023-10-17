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

#include "../../tasks/tasks.h"
namespace GP {
	void Peer::handle_addblock(OS::KVReader data_parser) {
		if (data_parser.HasKey("profileid")) {
			int profileid = data_parser.GetValueInt("profileid");
			if (std::find(m_blocks.begin(), m_blocks.end(), profileid) != m_blocks.end()) {
				send_error(GPShared::GP_ADDBLOCK_ALREADY_BLOCKED);
				return;
			}

			m_blocks.push_back(profileid);

			GPBackendRedisRequest req;
			req.type = EGPRedisRequestType_AddBlock;
			req.peer = this;
			req.peer->IncRef();
			req.ToFromData.to_profileid = profileid;
			req.ToFromData.from_profileid = m_profile.id;
			AddGPTaskRequest(req);
		} else {
			send_error(GPShared::GP_PARSE);
			return;
		}
		
	}
}