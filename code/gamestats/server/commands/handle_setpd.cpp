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

		//uv_mutex_lock(&m_mutex);
		bool profile_authenticated = std::find(m_authenticated_profileids.begin(), m_authenticated_profileids.end(), pid) != m_authenticated_profileids.end();
		//uv_mutex_unlock(&m_mutex);

		if(!profile_authenticated || persist_type == pd_public_ro || persist_type == pd_private_ro) {
			send_error(GPShared::GP_NOT_LOGGED_IN);
			return;
		}

		bool kv_set = data_parser.GetValueInt("kv") != 0;

		std::string data = data_parser.GetValue("data");
		int client_data_len = data_parser.GetValueInt("length");

		OS::KVReader save_data;
		const char *b64_str = NULL;

		if (kv_set) {
			save_data = data_parser.GetValue("data");
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

		PersistBackendRequest req;
		req.mp_peer = this;
		req.mp_extra = persist_request_data;
		req.type = EPersistRequestType_SetUserData;
		req.callback = setPersistDataCallback;
		req.data_type = persist_type;
		req.data_index = data_index;
		req.kv_set_data = save_data;
		req.data_kv_set = kv_set;
		req.profileid = pid;
		if (b64_str) {
			req.game_instance_identifier = b64_str;
			free((void *)b64_str);
		}
		
		IncRef();
		AddRequest(req);


		
	}
}
