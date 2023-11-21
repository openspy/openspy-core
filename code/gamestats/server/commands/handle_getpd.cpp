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
	void Peer::getPersistDataCallback(bool success, PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		//// \\getpdr\\1\\lid\\1\\length\\5\\data\\mydata\\final
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;

		std::ostringstream ss;

		uint8_t *data = NULL;
		size_t data_len = 0;
		
		ss << "\\getpdr\\" << success << "\\lid\\" << persist_request_data->operation_id << "\\pid\\" << persist_request_data->profileid;

		if(response_data.mod_time != 0) {
			 ss << "\\mod\\" << response_data.mod_time;
		}

		OS::Buffer buffer;
		buffer.WriteBuffer((void *)ss.str().c_str(), ss.str().length());
		if (response_data.game_instance_identifier.length() > 0) {

			OS::Base64StrToBin(response_data.game_instance_identifier.c_str(), &data, data_len);
			ss.str("");
			ss << "\\length\\" << data_len << "\\data\\";
			buffer.WriteBuffer((void *)ss.str().c_str(), ss.str().length());
			buffer.WriteBuffer(data, data_len);
		}
		else {
			std::pair<std::vector<std::pair< std::string, std::string> >::const_iterator, std::vector<std::pair< std::string, std::string> >::const_iterator> it = response_data.kv_data.GetHead();
			std::ostringstream kv_ss;
			while (it.first != it.second) {
				std::pair< std::string, std::string> item = *it.first;
				OS::Base64StrToBin(item.second.c_str(), &data, data_len);
				kv_ss << "\x01" << item.first << "\x01" << data;
				free((void *)data);
				data = NULL;
				it.first++;
			}
			data_len = kv_ss.str().length();

			ss.str("");
			ss << "\\length\\" << data_len << "\\data\\";
			buffer.WriteBuffer((void *)ss.str().c_str(), ss.str().length());

			std::string kv_str = kv_ss.str();

			//don't serialize in same format as recieved....
			/*for (int i = 0; i < kv_str.length(); i++) {
				if (kv_str[i] == '\\') {
					buffer.WriteByte(1);
				}
				else {
					buffer.WriteByte(kv_str[i]);
				}				
			}*/
			buffer.WriteBuffer((void *)kv_str.c_str(), kv_str.length());			
		}


		peer->SendOrWaitBuffer(persist_request_data->wait_index, peer->m_getpd_wait_ctx, buffer);
		//peer->SendPacket(buffer);

		if(data)
			free((void *)data);
		free((void *)persist_request_data);
	}
	void Peer::handle_getpd(OS::KVReader data_parser) {
		int operation_id = data_parser.GetValueInt("lid");
		int pid = data_parser.GetValueInt("pid");
		int data_index = data_parser.GetValueInt("dindex");

		int modified_since = data_parser.GetValueInt("mod");

		persisttype_t persist_type = (persisttype_t)data_parser.GetValueInt("ptype");

		if(persist_type == pd_private_ro || persist_type == pd_private_rw) {
			bool profile_authenticated = std::find(m_authenticated_profileids.begin(), m_authenticated_profileids.end(), pid) != m_authenticated_profileids.end();
			if (!profile_authenticated) {
				send_error(GPShared::GP_NOT_LOGGED_IN);
				return;
			}
		}
		switch(persist_type) {
			case pd_private_ro:
			case pd_private_rw:
				if(pid != m_profile.id)	{
					send_error(GPShared::GP_NOT_LOGGED_IN);
					return;
				}
			break;

			case pd_public_rw:
			case pd_public_ro:
			break;

			default:
			return;
		}

		std::string keys;
		std::vector<std::string> keyList;

		if (data_parser.HasKey("keys")) {
			keys = data_parser.GetValue("keys");
			for (size_t i = 0; i<keys.length(); i++) {
				if (keys[i] == '\x01') {
					keys[i] = '\\';
				}
			}
			keyList = OS::KeyStringToVector(keys);
		}

		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->profileid = pid;
		persist_request_data->operation_id = operation_id;
		persist_request_data->wait_index = m_get_request_index++;


		PersistBackendRequest req;
		req.mp_peer = this;
		req.mp_extra = persist_request_data;
		req.type = data_parser.HasKey("keys") ? EPersistRequestType_GetUserKeyedData : EPersistRequestType_GetUserData;
		req.callback = getPersistDataCallback;
		req.data_type = persist_type;
		req.data_index = data_index;
		req.keyList = keyList;
		req.profileid = pid;
		req.modified_since = modified_since;
		IncRef();
		AddRequest(req);
	}
}
