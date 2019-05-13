#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <OS/SharedTasks/tasks.h>
#include <tasks/tasks.h>


#include <server/Driver.h>
#include <server/Server.h>
#include <server/Peer.h>
namespace Peerchat {
    /*
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
    */
    void Peer::OnNickReserve(TaskResponse response_data, Peer *peer) {
        if(response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
            printf("VALID NICK\n");
        } else {
            printf("INVALID NICK\n");
        }
    }
    void Peer::handle_nick(std::vector<std::string> data_parser) {
        printf("got nick request: %s\n", data_parser.at(1).c_str());

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_ReserveNickname;
        req.peer = this;
        req.nick = data_parser.at(1);
        req.peer->IncRef();
        req.callback = OnNickReserve;
        scheduler->AddRequest(req.type, req);
    }
}