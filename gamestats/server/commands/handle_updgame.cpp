#include <server/GSServer.h>
#include <server/GSPeer.h>
#include <server/GSDriver.h>
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>

#include <stddef.h>
#include <sstream>
#include <algorithm>

#include <server/tasks/tasks.h>

#include <OS/GPShared.h>

#include <map>
#include <utility>

using namespace GPShared;

namespace GS {

	void Peer::updateGameCreateCallback(bool success, PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
	}
	void Peer::handle_updgame(OS::KVReader data_parser) {
		//\updgame\\sesskey\%d\done\%d\gamedata\%s
		int sesskey = data_parser.GetValueInt("sesskey");
		mp_mutex->lock();
		std::map<int, std::string>::iterator it = m_game_session_backend_identifier_map.find(sesskey);

		//not found... must be a pending request XXX: check into this logic more!!
		if(it == m_game_session_backend_identifier_map.end()) {
			m_updgame_sesskey_wait_list[sesskey].push_back(data_parser);
			this->IncRef();
			m_updgame_increfs++;
			if (m_updgame_sesskey_wait_list[sesskey].size() > MAX_SESSKEY_WAIT || m_updgame_sesskey_wait_list.size() > MAX_SESSKEY_WAIT) {				
				for (int i = 0; i < m_updgame_increfs; i++) {
					this->DecRef();
				}
				send_error(GPShared::GP_BAD_SESSKEY);
			}
			mp_mutex->unlock();
			return;
		}
		std::map<std::string,std::string> game_data;
		std::string gamedata = data_parser.GetValue("gamedata");

		bool done = data_parser.GetValueInt("done");

		for(size_t i=0;i<gamedata.length();i++) {
			if(gamedata[i] == '\x1') {
				gamedata[i] = '\\';
			}
		}

		game_data = OS::KeyStringToMap(gamedata);
		TaskScheduler<PersistBackendRequest, TaskThreadData> *scheduler = ((GS::Server *)(GetDriver()->getServer()))->GetGamestatsTask();
		PersistBackendRequest req;
		req.mp_peer = this;
		req.mp_extra = NULL;
		req.type = EPersistRequestType_UpdateGame;
		req.callback = updateGameCreateCallback;
		req.complete = done;
		req.kvMap = game_data;
		req.game_instance_identifier = m_game_session_backend_identifier_map[sesskey];
		IncRef();
		scheduler->AddRequest(req.type, req);

		mp_mutex->unlock();
	}
}