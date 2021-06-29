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
    void Peer::m_login_auth_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
        std::ostringstream s;
        if(auth_data.error_details.response_code == TaskShared::WebErrorCode_Success) {
            ((Peer *)peer)->m_profile = profile;
            ((Peer *)peer)->m_user = user;
            ((Peer *)peer)->m_user_details.profileid = profile.id;
            s << user.id << " " << profile.id;
            ((Peer *)peer)->send_numeric(707, s.str());

            ((Peer *)peer)->perform_oper_check();
        } else {
            s << "Invalid login information (" << profile.id << ")";
            ((Peer *)peer)->send_numeric(708, s.str());
        }
    }
    void Peer::handle_login(std::vector<std::string> data_parser) {
        TaskShared::AuthRequest request;
        
		request.callback = m_login_auth_cb;
		request.peer = this;
		IncRef();

        request.profile.namespaceid = atoi(data_parser.at(1).c_str());
        request.user.password = data_parser.at(3);
        if(data_parser.size() == 4) { //uniquenick login            
            request.profile.uniquenick = data_parser.at(2);            
            request.type = TaskShared::EAuthType_Uniquenick_Password;
        } else if(data_parser.size() == 5) { //profile login
            request.type = TaskShared::EAuthType_User_EmailPassword;
            std::string nick_email_str = data_parser.at(4);
            if(nick_email_str[0] == ':') {
                nick_email_str = nick_email_str.substr(1);
            }

            const char *nick_email = nick_email_str.c_str();
            const char *email = NULL;
            char nick[31 + 1];
            const char *first_at = strchr(nick_email, '@');
            if(first_at) {
                unsigned int nick_len = (unsigned int)(first_at - nick_email);
                if(nick_len < sizeof(nick)) {
                    strncpy(nick, nick_email, nick_len);
                    nick[nick_len] = 0;
                }
                email = first_at+1;
            }

            request.user.email = email;
            request.profile.nick = nick;
        }
        
        request.user.partnercode = OS_GAMESPY_PARTNER_CODE;

		TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetAuthTask();
		scheduler->AddRequest(request.type, request);
    }

    void Peer::handle_loginpreauth(std::vector<std::string> data_parser) {
        TaskShared::AuthRequest request;
        
		request.callback = m_login_auth_cb;
		request.peer = this;
		IncRef();

        request.auth_token = data_parser.at(1);
        request.auth_token_challenge = data_parser.at(2);

        request.type = TaskShared::EAuthType_TestPreAuth;

        TaskScheduler<TaskShared::AuthRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetAuthTask();
		scheduler->AddRequest(request.type, request);

        
    }
}