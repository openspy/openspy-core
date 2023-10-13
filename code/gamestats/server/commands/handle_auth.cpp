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
	void Peer::onGetGameDataCallback(bool success, PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		std::ostringstream ss;
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;
		peer->m_game = response_data.gameData;
		if(peer->m_game.secretkey[0] == 0) {
			peer->send_error(GPShared::GP_CONNECTION_CLOSED);
			return;
		}

		if(!peer->IsResponseValid(peer->m_response.c_str())) {
			peer->send_error(GPShared::GP_CONNECTION_CLOSED);
			peer->Delete();
			return;
		}

		ss << "\\lc\\2\\sesskey\\" << peer->m_session_key << "\\proof\\0\\id\\" << persist_request_data->operation_id;
		peer->SendPacket(ss.str());
	}
	void Peer::handle_auth(OS::KVReader data_parser) {
		std::string gamename;
		std::string response;
		int local_id = data_parser.GetValueInt("id");
		m_game_port = data_parser.GetValueInt("port");

		if (!data_parser.HasKey("gamename")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		gamename = data_parser.GetValue("gamename");

		if (!data_parser.HasKey("response")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		m_response = data_parser.GetValue("response");


		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->profileid = 0;
		persist_request_data->operation_id = local_id;

		PersistBackendRequest req;
		req.mp_peer = this;
		req.mp_extra = persist_request_data;
		req.type = EPersistRequestType_GetGameInfoByGamename;
		req.callback = onGetGameDataCallback;
		req.game_instance_identifier = gamename;
		IncRef();
		AddRequest(req);
	}
}