#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>


#include <server/Driver.h>
#include <server/Server.h>
#include <server/Peer.h>

namespace Peerchat {
	void Peer::OnGetGameInfo_Crypt(TaskResponse response_data, Peer* peer) {
        if(response_data.game_data.secretkey[0] == 0) {
            peer->Delete(false, "Invalid gamename");
            return;
        }

        std::ostringstream ss;

        char client_challenge[17], server_challenge[17];

        memset(&client_challenge, 0, sizeof(client_challenge));
        OS::gen_random((char *)&client_challenge, sizeof(client_challenge)-1);

        memset(&server_challenge, 0, sizeof(server_challenge));
        OS::gen_random((char *)&server_challenge, sizeof(server_challenge)-1);

        ss << client_challenge << " " << server_challenge;

        const char *secretkey = response_data.game_data.secretkey.c_str();
        
        gs_peerchat_init(&peer->m_crypt_key_in,  (unsigned char *)&client_challenge, (unsigned char *) secretkey);
        gs_peerchat_init( &peer->m_crypt_key_out,  (unsigned char *)&server_challenge, (unsigned char *)secretkey);

        peer->send_numeric(705, ss.str(), true);

        peer->m_using_encryption = true;
        peer->m_user_details.gameid = response_data.game_data.gameid;

        
        PeerchatBackendRequest req;
        req.type = EPeerchatRequestType_SetUserDetails;
        req.peer = peer;
        req.summary = peer->GetUserDetails();
        req.peer->IncRef();
        req.callback = NULL;
        AddPeerchatTaskRequest(req);

    }

    void Peer::handle_crypt(std::vector<std::string> data_parser) {
        std::string gamename = data_parser.at(3);

        PeerchatBackendRequest req;

		req.type = EPeerchatRequestType_LookupGameInfo;
		req.peer = this;
        req.gamename = gamename;

		req.peer->IncRef();
		req.callback = OnGetGameInfo_Crypt;
		AddPeerchatTaskRequest(req);
    }
}