#include <server/GSServer.h>
#include <server/GSPeer.h>
#include <server/GSDriver.h>
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>

#include <stddef.h>
#include <sstream>
#include <algorithm>

#include <server/tasks/tasks.h>

#include <OS/GPShared.h>

#include <map>
#include <utility>

using namespace GPShared;

namespace GS {
	void Peer::m_getpid_cb(bool success, OS::User user, OS::Profile profile, TaskShared::AuthData auth_data, void *extra, INetPeer *peer) {
		std::ostringstream ss;
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;
		int operation_id = persist_request_data->operation_id;


		if (success) {
			ss << "\\getpidr\\" << profile.id;

			ss << "\\lid\\" << operation_id;

			((Peer *)peer)->m_profile = profile;
			((Peer *)peer)->m_user = user;

			((Peer *)peer)->SendPacket(ss.str());

		}
		else {
			ss << "\\getpidr\\-1\\errmsg\\";
			GPShared::GPErrorData error_data;
			GPShared::GPErrorCode code;

			switch (auth_data.error_details.response_code) {
			case TaskShared::WebErrorCode_BadCdKey:
				code = GP_REGISTERCDKEY_BAD_KEY;
			break;
			case TaskShared::WebErrorCode_NoSuchUser:
				//code = GP_LOGIN_BAD_EMAIL;
				code = GP_LOGIN_BAD_PROFILE;
				break;
			case TaskShared::WebErrorCode_AuthInvalidCredentials:
				code = GP_LOGIN_BAD_PASSWORD;
				break;
			case TaskShared::WebErrorCode_UniqueNickInvalid:
				code = GP_LOGIN_BAD_UNIQUENICK;
				break;
			default:
				code = GP_DATABASE;
				break;
			}
			error_data = GPShared::getErrorDataByCode(code);

			ss << error_data.msg;

			ss << "\\lid\\" << operation_id;
			((Peer *)peer)->SendPacket(ss.str());
		}
	}

	//\getpid\\nick\crt\keyhash\00000a308fd86a7eb92cbc8322b03a36\lid\1
    void Peer::handle_getpid(OS::KVReader data_parser) {
		/*
				Send error response until implemented
		*/
		/*int operation_id = data_parser.GetValueInt("lid");
		//int pid = data_parser.GetValueInt("pid");
		std::ostringstream ss;
		ss << "\\getpidr\\-1\\lid\\" << operation_id;
		SendPacket(ss.str());*/

		std::string nick, cdkey;

		if (!data_parser.HasKey("nick")) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		if (!data_parser.HasKey("keyhash")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		if (!data_parser.HasKey("lid")) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		int operation_id = data_parser.GetValueInt("lid");

		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->wait_index = m_getpid_request_index++;
		persist_request_data->operation_id = operation_id;

		TaskScheduler<PersistBackendRequest, TaskThreadData> *scheduler = ((GS::Server *)(GetDriver()->getServer()))->GetGamestatsTask();
		PersistBackendRequest req;
		req.mp_peer = this;
		req.mp_extra = persist_request_data;
		req.type = EPersistRequestType_GetProfileIDFromCDKey;
		req.authCallback = m_getpid_cb;
		req.auth_token = data_parser.GetValue("keyhash");
		req.profile_nick = data_parser.GetValue("nick");
		req.profileid = m_game.gameid;

		IncRef();
		scheduler->AddRequest(req.type, req);
	}
}