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
	void Peer::OnGetGameInfo_Crypt(TaskResponse response_data, Peer* peer) {
        if(response_data.game_data.gameid == 0) {
            peer->Delete(false, "Invalid gamename");
            return;
        }

        std::ostringstream ss;

        char client_challenge[17], server_challenge[17];
        strcpy(client_challenge, "NtJ0ID3rWuIGbskv");
        strcpy(server_challenge, "p2A9g9CFtwgGlPI8");

        ss << client_challenge << " " << server_challenge;

        gs_xcode_buf(client_challenge, 16, response_data.game_data.secretkey.c_str());
        gs_xcode_buf(server_challenge, 16, response_data.game_data.secretkey.c_str());
        
        gs_prepare_key(client_challenge, 16, &peer->m_crypt_key_in);
        gs_prepare_key(server_challenge, 16, &peer->m_crypt_key_out);
        peer->send_numeric(705, ss.str(), true);

        peer->m_using_encryption = true;
    }

    void Peer::handle_crypt(std::vector<std::string> data_parser) {
        std::string gamename = data_parser.at(3);

        PeerchatBackendRequest req;

        TaskScheduler<PeerchatBackendRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetPeerchatTask();

		req.type = EPeerchatRequestType_LookupGameInfo;
		req.peer = this;
        req.gamename = gamename;

		req.peer->IncRef();
		req.callback = OnGetGameInfo_Crypt;
		scheduler->AddRequest(req.type, req);


    }
}