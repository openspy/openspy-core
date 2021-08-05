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
	void Peer::m_update_registernick_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		std::ostringstream s;
		s << "\\id\\" << (int)extra;
		std::string id_string = s.str();
		s.str("");
		GP::Peer *gppeer = (GP::Peer *)peer;
		switch (error_details.response_code) {
			case TaskShared::WebErrorCode_UniqueNickInUse:
				gppeer->send_error(GPShared::GP_REGISTERUNIQUENICK_TAKEN, id_string);
				break;
			default:
			case TaskShared::WebErrorCode_CdKeyAlreadyTaken:
			case TaskShared::WebErrorCode_BadCdKey:
				gppeer->send_error(GPShared::GP_REGISTERUNIQUENICK, id_string);
				break;
			case TaskShared::WebErrorCode_Success:
				break;
		}
		if (error_details.response_code != TaskShared::WebErrorCode_Success) {
			return;
		}
		
		s << "\\rn\\1" << id_string;

		gppeer->SendPacket((const uint8_t *)s.str().c_str(), s.str().length());
	}
	void Peer::handle_registernick(OS::KVReader data_parser) {
        std::string uniquenick, cdkey;
		int id = data_parser.GetValueInt("id");

        if(data_parser.HasKey("cdkey")) {
            cdkey = data_parser.GetValue("cdkey");
        }

        if(data_parser.HasKey("uniquenick")) {
            uniquenick = data_parser.GetValue("uniquenick");
        } else {
            send_error(GPShared::GP_PARSE);
            return;
        }

		TaskShared::ProfileRequest request;
		request.profile_search_details = m_profile;
        //request.profile_search_details.cdkey = cdkey;
        request.profile_search_details.uniquenick = uniquenick;
		request.extra = (void *)id;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::EProfileSearch_UpdateProfile;
		request.callback = Peer::m_update_registernick_callback;
		TaskScheduler<TaskShared::ProfileRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetProfileTask();
		scheduler->AddRequest(request.type, request);
	}
}