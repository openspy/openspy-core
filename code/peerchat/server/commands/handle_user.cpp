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
    void Peer::handle_user(std::vector<std::string> data_parser) {
        

        m_user_details.username = data_parser.at(1);
        m_user_details.realname = data_parser.at(4);
        if(m_user_details.realname.at(0) == ':') {
            m_user_details.realname = m_user_details.realname.substr(1);
        }

        OnUserMaybeRegistered();
    }
}