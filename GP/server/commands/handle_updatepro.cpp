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
	/*
		Updates profile specific information
	*/
	void Peer::handle_updatepro(OS::KVReader data_parser) {
		bool send_userupdate = false;

		//OS::KVReader data_parser = OS::KVReader(std::string(data));
		OS::Profile profile = m_profile;
		OS::User user = m_user;

		LoadParamInt("zipcode", profile.zipcode, data_parser)
		LoadParamInt("sex", profile.sex, data_parser)
		LoadParamInt("pic", profile.pic, data_parser)
		LoadParamInt("ooc", profile.ooc, data_parser)
		LoadParamInt("ind", profile.ind, data_parser)
		LoadParamInt("mar", profile.mar, data_parser)
		LoadParamInt("chc", profile.chc, data_parser)
		LoadParamInt("i1", profile.i1, data_parser)
		LoadParamInt("publicmask", user.publicmask, data_parser) //user param

		if (data_parser.HasKey("birthday")) {
			profile.birthday = OS::Date::GetDateFromGPValue(data_parser.GetValueInt("birthday"));

		}

		if (data_parser.HasKey("publicmask")) {
				send_userupdate = true;
		}
		LoadParam("nick", profile.nick, data_parser)
		LoadParam("uniquenick", profile.uniquenick, data_parser)
		LoadParam("firstname", profile.firstname, data_parser)
		LoadParam("lastname", profile.lastname, data_parser)
		LoadParam("countrycode", profile.countrycode, data_parser)
		LoadParam("videocard1string", profile.videocardstring[0], data_parser)
		LoadParam("videocard2string", profile.videocardstring[1], data_parser)
		LoadParam("osstring", profile.osstring, data_parser)
		LoadParam("aim", profile.aim, data_parser)

		TaskShared::ProfileRequest request;
		request.profile_search_details = profile;
		request.extra = NULL;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_UpdateProfile;
		request.callback = Peer::m_update_profile_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);

		if(send_userupdate) {
			TaskShared::UserRequest user_request;
			user_request.search_params = user;
			user_request.type = TaskShared::EUserRequestType_Update;
			user_request.extra = NULL;
			user_request.peer = this;
			user_request.peer->IncRef();
			user_request.callback = NULL;
			TaskScheduler<TaskShared::UserRequest, TaskThreadData> *user_scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetUserTask();
			user_scheduler->AddRequest(user_request.type, user_request);
		}
	}
	void Peer::m_update_profile_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		/* fake testing response... no real response in GP
		std::ostringstream s;
		s << "\\pui\\1";
		s << "\\result_size\\" << results.size();
		((GP::Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(), s.str().length());*/
		//s << "\\id\\" << (int)extra;
	}
}