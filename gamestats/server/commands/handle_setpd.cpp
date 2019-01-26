#include <server/GSServer.h>
#include <server/GSPeer.h>
#include <server/GSDriver.h>
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <OS/Search/Profile.h>

#include <OS/Search/User.h>

#include <stddef.h>
#include <sstream>
#include <algorithm>

#include <server/tasks/tasks.h>

#include <OS/GPShared.h>

#include <map>
#include <utility>

using namespace GPShared;

namespace GS {
	void Peer::setPersistDataCallback(bool success, PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;

		std::ostringstream ss;
		ss << "\\setpdr\\" << success << "\\lid\\" << persist_request_data->operation_id << "\\pid\\" << persist_request_data->profileid << "\\mod\\" << response_data.mod_time;
		OS::Buffer buffer;
		buffer.WriteBuffer((void *)ss.str().c_str(), ss.str().length());
		peer->SendOrWaitBuffer(persist_request_data->wait_index, peer->m_setpd_wait_ctx, buffer);

		free((void *)persist_request_data);
	}

	void Peer::handle_setpd(OS::KVReader data_parser) {
		int operation_id = data_parser.GetValueInt("lid");
		int pid = data_parser.GetValueInt("pid");
		int data_index = data_parser.GetValueInt("dindex");

		persisttype_t persist_type = (persisttype_t)data_parser.GetValueInt("ptype");


		if(pid != m_profile.id || persist_type == pd_public_ro || persist_type == pd_private_ro) {
			send_error(GPShared::GP_NOT_LOGGED_IN);
			return;
		}

		bool kv_set = data_parser.GetValueInt("kv") != 0;

		std::string data = data_parser.GetValue("data");
		int client_data_len = data_parser.GetValueInt("length");

		OS::KVReader save_data;
		const char *b64_str = NULL;

		if (kv_set) {
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> it = data_parser.GetHead();
			bool found_end = false;
			std::ostringstream ss;
			while (it.first != it.second) {
				std::pair< std::string, std::string> item = *it.first;
				
				if (found_end) {
					ss << "\\" << item.first << "\\" << item.second;
				}
				if (item.first.compare("data") == 0) {
					found_end = true;
				}
				it.first++;
			}

			data = ss.str();
			save_data = data;
			client_data_len--;
		}
		else {
			b64_str = OS::BinToBase64Str((uint8_t *)data.c_str(), client_data_len);
		}

		/* Some games send data-1... but its safe now with the new reader
		if (!data.length() || client_data_len != data.length()) {
			send_error(GPShared::GP_PARSE);
			return;
		}*/

		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->profileid = pid;
		persist_request_data->operation_id = operation_id;
		persist_request_data->wait_index = m_set_request_index++;

		TaskScheduler<PersistBackendRequest, TaskThreadData> *scheduler = ((GS::Server *)(GetDriver()->getServer()))->GetGamestatsTask();
		PersistBackendRequest req;
		req.mp_peer = this;
		req.mp_extra = persist_request_data;
		req.type = EPersistRequestType_SetUserData;
		req.callback = setPersistDataCallback;
		req.data_type = persist_type;
		req.data_index = data_index;
		req.kv_set_data = save_data;
		req.profileid = m_profile.id;
		req.game_instance_identifier = b64_str;
		IncRef();
		scheduler->AddRequest(req.type, req);


		free((void *)b64_str);
	}
}