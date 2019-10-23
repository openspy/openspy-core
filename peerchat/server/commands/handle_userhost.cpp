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
	void Peer::handle_userhost(std::vector<std::string> data_parser) {
		std::ostringstream ss;
		ss << m_user_details.nick << "=+" << m_user_details.username << "@" << m_user_details.hostname;
		send_numeric(302, ss.str());
	}
}