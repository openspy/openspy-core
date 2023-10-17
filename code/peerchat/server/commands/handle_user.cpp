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
    void Peer::handle_user(std::vector<std::string> data_parser) {
        
        std::string username, realname;

        username = data_parser.at(1);
        realname = data_parser.at(4);
        if(realname.length() > 1 && realname.at(0) == ':') {
            realname = realname.substr(1);
        }

        if(is_nick_valid(username.c_str()) == 0) {
            send_numeric(468, "Erroneous User", false, username);
            return;
        }
        if(is_nick_valid(realname.c_str()) == 0) {
            send_numeric(468, "Erroneous Realname", false, realname);
            return;
        }

        m_user_details.username = username;
        m_user_details.realname = realname;

        OnUserMaybeRegistered();
    }
}