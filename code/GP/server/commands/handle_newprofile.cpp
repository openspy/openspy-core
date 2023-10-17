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
	class NewProfileInfo {
		public:
			OS::Profile deleted_profile;
			int operation_id;
	} ;
	void Peer::m_create_profile_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		NewProfileInfo *info = (NewProfileInfo*)extra;
		std::ostringstream s;
		if(error_details.response_code == TaskShared::WebErrorCode_Success && results.size() > 0) {
			OS::Profile profile = results.front();
			s << "\\npr\\1";
			s << "\\profileid\\" << profile.id;
			s << "\\id\\" << info->operation_id;

			((GP::Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(), s.str().length());
		}
		else {
			s << "\\id\\" << info->operation_id;
			switch (error_details.response_code) {
				case TaskShared::WebErrorCode_NickInUse:
					((GP::Peer *)peer)->send_error(GPShared::GP_NEWPROFILE_BAD_NICK, s.str());
				break;
				default:
					((GP::Peer *)peer)->send_error(GPShared::GP_NEWPROFILE, s.str());
				break;
			}
			
		}

		delete info;
	}
	void Peer::m_create_profile_replace_update_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		NewProfileInfo *info = (NewProfileInfo*)extra;
		if (results.size() == 0) {
			((GP::Peer *)peer)->send_error(GPShared::GP_NEWPROFILE_BAD_OLD_NICK);
			delete info;
			return;
		}
		
		OS::Profile profile = results.front();
		std::ostringstream s;
		s << "\\npr\\1";
		s << "\\profileid\\" << profile.id;
		s << "\\id\\" << info->operation_id;

		((GP::Peer *)peer)->SendPacket((const uint8_t *)s.str().c_str(), s.str().length());
		delete info;
	}
	void Peer::m_create_profile_replace_delete_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		NewProfileInfo *info = (NewProfileInfo*)extra;

		TaskShared::ProfileRequest request;
		//request.profile_search_details.id = m_profile.id;
		request.profile_search_details = ((GP::Peer *)peer)->m_profile;
		request.profile_search_details.nick = info->deleted_profile.nick;
		request.user_search_details.id = request.profile_search_details.userid;
		request.extra = (void *)info;
		request.peer = peer;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_UpdateProfile;
		request.callback = Peer::m_create_profile_replace_update_callback;


		AddProfileTaskRequest(request);

		//free(extra);
	}
	void Peer::m_create_profile_replace_lookup_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		NewProfileInfo *info = (NewProfileInfo*)extra;
		if (error_details.response_code == TaskShared::WebErrorCode_Success && results.size() > 0) {
			OS::Profile profile = results.front();

			TaskShared::ProfileRequest request;
			request.profile_search_details.id = profile.id;
			request.profile_search_details.userid = ((GP::Peer *)peer)->m_user.id;
			request.user_search_details.id = ((GP::Peer *)peer)->m_user.id;
			request.extra = extra;
			request.peer = peer;
			request.peer->IncRef();
			request.type = TaskShared::EProfileSearch_DeleteProfile;
			request.callback = Peer::m_create_profile_replace_delete_callback;

			info->deleted_profile = profile;

			AddProfileTaskRequest(request);
		}
	}
	void Peer::handle_newprofile(OS::KVReader data_parser) {
		TaskShared::ProfileRequest request;
		int replace = data_parser.GetValueInt("replace");
		std::string nick, oldnick;
		nick = data_parser.GetValue("nick");

		NewProfileInfo *info = new NewProfileInfo;
		info->operation_id = data_parser.GetValueInt("id");
		
		if(replace) { //TODO: figure out replaces functionality
			oldnick = data_parser.GetValue("oldnick");

			request.profile_search_details.nick = oldnick;
			request.profile_search_details.userid = m_user.id;
			request.user_search_details.id = m_user.id;
			request.extra = (void *)info;
			request.peer = this;
			request.peer->IncRef();
			request.type = TaskShared::EProfileSearch_Profiles;
			request.callback = Peer::m_create_profile_replace_lookup_callback;
		}
		else {
			//request.profile_search_details.id = m_profile.id;
			request.profile_search_details.nick = nick;
			request.profile_search_details.userid = m_user.id;
			request.user_search_details.id = m_user.id;
			request.extra = (void *)info;
			request.peer = this;
			request.peer->IncRef();
			request.type = TaskShared::EProfileSearch_CreateProfile;
			request.callback = Peer::m_create_profile_callback;
		}


		AddProfileTaskRequest(request);
	}
}