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
	void Peer::m_registernick_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;
		if (error_details.response_code == TaskShared::WebErrorCode_Success) {
			return;
		}
        s << 0 << " " << "\\"; //XXX: lookup suggestions, no suggestions currently
		((Peer *)peer)->send_numeric(711, s.str());

	}
    void Peer::handle_registernick(std::vector<std::string> data_parser) {
		TaskShared::ProfileRequest request;

        std::string uniquenick = data_parser.at(2);
        int namespaceid = atoi(data_parser.at(1).c_str());
		request.profile_search_details = m_profile;
        request.profile_search_details.uniquenick = uniquenick;
        request.profile_search_details.namespaceid = namespaceid;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_UpdateProfile;
		request.callback = Peer::m_registernick_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((Peerchat::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
    }
}