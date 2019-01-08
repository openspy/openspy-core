#include <GP/server/GPPeer.h>
#include <GP/server/GPDriver.h>
#include <GP/server/GPServer.h>
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>

#include <OS/Search/User.h>

#include <OS/Buffer.h>
#include <OS/KVReader.h>
#include <sstream>
#include <algorithm>

#include <OS/gamespy/gamespy.h>
#include <tasks/tasks.h>

namespace GP {
	void Peer::m_create_profile_callback(TaskShared::EProfileResponseType response_reason, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		if(response_reason == OS::EProfileResponseType_Success && results.size() > 0) {
			OS::Profile profile = results.front();
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
		request.profile_search_details.id = m_profile.id;
		request.profile_search_details.nick = nick;
		request.extra = this;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_CreateProfile;
		request.callback = Peer::m_create_profile_callback;

		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
}