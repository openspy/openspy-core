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
	void Peer::m_set_cdkey_callback(TaskShared::CDKeyData auth_data, void *extra, INetPeer *peer) {
		GP::Peer *gppeer = (GP::Peer *)peer;
		switch (auth_data.error_details.response_code) {
			case TaskShared::WebErrorCode_CdKeyAlreadySet:
				gppeer->send_error(GPShared::GP_REGISTERCDKEY_ALREADY_SET);
				break;
			case TaskShared::WebErrorCode_CdKeyAlreadyTaken:
				gppeer->send_error(GPShared::GP_REGISTERCDKEY_ALREADY_TAKEN);
				break;
			case TaskShared::WebErrorCode_BadCdKey:
				gppeer->send_error(GPShared::GP_REGISTERCDKEY_BAD_KEY);
				break;
		}
		if (auth_data.error_details.response_code != TaskShared::WebErrorCode_Success) {
			return;
		}
		std::ostringstream s;
		s << "\\rc\\1";
		s << "\\id\\" << (int)extra;

		gppeer->SendPacket((const uint8_t *)s.str().c_str(), s.str().length());
	}
	void Peer::handle_registercdkey(OS::KVReader data_parser) {
        std::string cdkey;
		int id = data_parser.GetValueInt("id");

        if(data_parser.HasKey("cdkey")) {
            cdkey = data_parser.GetValue("cdkey");
		}
		else if (data_parser.HasKey("cdkeyenc")) {
			std::string cdkeyenc = data_parser.GetValue("cdkeyenc");
			int passlen = (int)cdkeyenc.length();
			char *dpass = (char *)base64_decode((uint8_t *)cdkeyenc.c_str(), &passlen);
			passlen = gspassenc((uint8_t *)dpass, passlen);
			cdkey = dpass;
			if (dpass)
				free((void *)dpass);
		}

		TaskShared::CDKeyRequest request;
		if (data_parser.HasKey("gameid")) {
			request.gameid = data_parser.GetValueInt("gameid");
		}
		request.cdkey = cdkey;
		request.profile = m_profile;
		request.extra = (void *)id;
		request.peer = this;
		request.peer->IncRef();
		request.type = TaskShared::ECDKeyType_AssociateToProfile;
		request.callback = Peer::m_set_cdkey_callback;
		TaskScheduler<TaskShared::CDKeyRequest, TaskThreadData> *scheduler = ((GP::Server *)(GetDriver()->getServer()))->GetCDKeyTasks();
		scheduler->AddRequest(request.type, request);
	}
}