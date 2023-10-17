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
    void Peer::OnFetchOperStatus(TaskResponse response_data, Peer* peer) {
        if(response_data.error_details.response_code == TaskShared::WebErrorCode_Success) {            
            ((Peer *)peer)->m_user_details.operflags = response_data.channel_summary.basic_mode_flags;
            if(((Peer *)peer)->m_user_details.operflags != 0) {
                ((Peer *)peer)->send_message("NOTICE", "Rights Granted", *server_userSummary, ((Peer *)peer)->m_user_details.nick);
            }
            
        } else {
            ((Peer *)peer)->m_user_details.operflags = 0;
        }
    }
    void Peer::perform_oper_check() {
        refresh_user_details();
        
		PeerchatBackendRequest request;
		request.type = EPeerchatRequestType_OperCheck;
		request.callback = OnFetchOperStatus;
		request.peer = this;
		IncRef();
        request.profile.id = m_profile.id;
        

		AddPeerchatTaskRequest(request);
    }
    void Peer::m_oper_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
        if(auth_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
            ((Peer *)peer)->m_profile = profile;
            ((Peer *)peer)->m_user = user;
            ((Peer *)peer)->m_user_details.operflags = 0;
            ((Peer *)peer)->m_user_details.profileid = profile.id;
            ((Peer *)peer)->m_user_details.userid = user.id;
            ((Peer *)peer)->send_message("NOTICE", "Authenticated", *server_userSummary, ((Peer *)peer)->m_user_details.nick);
            ((Peer *)peer)->perform_oper_check();
        } else {
            
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

		AddAuthTaskRequest(request);
    }
}