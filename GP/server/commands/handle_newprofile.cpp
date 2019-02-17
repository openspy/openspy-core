#include <OS/OpenSpy.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>

#include <GP/server/GPPeer.h>
#include <GP/server/GPDriver.h>
#include <GP/server/GPServer.h>

namespace GP {
	void Peer::m_create_profile_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		if(error_details.response_code == TaskShared::WebErrorCode_Success && results.size() > 0) {
			OS::Profile profile = results.front();
			std::ostringstream s;
			s << "\\npr\\1";
			s << "\\profileid\\" << profile.id;
			s << "\\id\\" << (int)extra;

			((GP::Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(), s.str().length());
		}
		else {
			((GP::Peer *)peer)->send_error(GPShared::GP_NEWPROFILE);
		}
	}
	void Peer::handle_newprofile(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		int replace = data_parser.GetValueInt("replace");
		std::string nick, oldnick;
		nick = data_parser.GetValue("nick");
		
		if(replace) { //TODO: figure out replaces functionality
			oldnick = data_parser.GetValue("oldnick");
		}
		//request.profile_search_details.id = m_profile.id;
		request.profile_search_details.nick = nick;
		request.profile_search_details.userid = m_user.id;
		request.user_search_details.id = m_user.id;
		request.extra = (void *)data_parser.GetValueInt("id");
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_CreateProfile;
		request.callback = Peer::m_create_profile_callback;

		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
}