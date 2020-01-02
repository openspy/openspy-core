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
    void Peer::m_oper_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
        if(auth_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
            ((Peer *)peer)->m_profile = profile;
            ((Peer *)peer)->m_user = user;
            ((Peer *)peer)->send_message("NOTICE", "RIGHTS GRANTED", "SERVER!SERVER@*", ((Peer *)peer)->m_user_details.nick);
        } else {
            printf("LOG IN FAILED\n");
        }
    }
    void Peer::handle_oper(std::vector<std::string> data_parser) {
		TaskShared::AuthRequest request;
		request.type = TaskShared::EAuthType_Uniquenick_Password;
		request.callback = m_oper_auth_cb;
		request.peer = this;
		IncRef();
        request.profile.uniquenick = data_parser.at(1);
        request.user.email = data_parser.at(2);
        request.user.password = data_parser.at(3);

        request.user.partnercode = OS_GAMESPY_PARTNER_CODE;
        request.profile.namespaceid = 1;

		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetAuthTask();
		scheduler->AddRequest(request.type, request);
    }
}