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
	void Peer::newGameCreateCallback(bool success, PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		peer->mp_mutex->lock();
		size_t sesskey = (size_t)extra;
		peer->m_game_session_backend_identifier_map[sesskey] = response_data.game_instance_identifier;
		
		
		
		std::map<int, std::vector<OS::KVReader> >::iterator it = peer->m_updgame_sesskey_wait_list.find(sesskey);
		if (it != peer->m_updgame_sesskey_wait_list.end()) {
			std::pair<int, std::vector<OS::KVReader> > p = *it;
			std::vector<OS::KVReader>::iterator it2 = p.second.begin();
			while (it2 != p.second.end()) {
				peer->handle_updgame(*it2);
				peer->DecRef();
				peer->m_updgame_increfs--;
				it2++;
			}
			peer->m_updgame_sesskey_wait_list.erase(it);
		}
		peer->mp_mutex->unlock();

	}
	void Peer::handle_newgame(OS::KVReader data_parser) {
		size_t session_id = (size_t)data_parser.GetValueInt("sesskey"); //identifier, used incase of multiple game sessions happening at once
		PersistBackendRequest req;
		req.mp_peer = this;
		req.mp_extra = (void *)session_id;
		req.profileid = m_profile.id;
		req.type = EPersistRequestType_NewGame;
		req.callback = newGameCreateCallback;
		IncRef();
		AddRequest(req);
	}
}