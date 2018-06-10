#include "GSPeer.h"
#include "GSDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Buffer.h>
#include <OS/Search/Profile.h>

#include <OS/Search/User.h>

#include <sstream>
#include <algorithm>

#include "GSBackend.h"

#include <OS/GPShared.h>

#include <map>
#include <utility>

using namespace GPShared;

namespace GS {

	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;
		
		mp_mutex = OS::CreateMutex();

		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);

		m_user.id = 0;
		m_profile.id = 0;

		m_session_key = 1234567;

		OS::gen_random(m_challenge, CHALLENGE_LEN);

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection",m_sd->address.ToString().c_str());

		send_login_challenge(1);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed", m_sd->address.ToString().c_str());
		delete mp_mutex;
	}
	void Peer::send_login_challenge(int type) {
		std::ostringstream s;

		s << "\\lc\\" << type;
		switch(type) {
			case 1: {
				s << "\\challenge\\" << m_challenge;
				s << "\\id\\" << GPI_CONNECTING;
				break;
			}
		}
		SendPacket(s.str());
	}
	void Peer::think(bool packet_waiting) {
		
		NetIOCommResp io_resp;
		if (m_delete_flag) return;
		if (packet_waiting) {
			io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamRecv(m_sd, m_recv_buffer);
			int len = io_resp.comm_len;

			if ((io_resp.disconnect_flag || io_resp.error_flag) || len <= 0) {
				goto end;
			}
			
			gamespy3dxor((char *)m_recv_buffer.GetHead(), len);

			/*
				This scans the incoming packets for \\final\\ and splits based on that,
				
				
				as well as handles incomplete packets -- due to TCP preserving data order, this is possible, and cannot be used on UDP protocols				
			*/
			std::string recv_buf = m_kv_accumulator;
			m_kv_accumulator.clear();
			recv_buf.append((const char *)m_recv_buffer.GetHead(), len);

			size_t final_pos = 0, last_pos = 0;

			do {
				final_pos = recv_buf.find("\\final\\", last_pos);
				if (final_pos == std::string::npos) break;

				std::string partial_string = recv_buf.substr(last_pos, final_pos - last_pos);
				handle_packet(partial_string);

				last_pos = final_pos + 7; // 7 = strlen of \\final
			} while (final_pos != std::string::npos);


			//check for extra data that didn't have the final string -- incase of incomplete data
			if (last_pos < len) {
				std::string remaining_str = recv_buf.substr(last_pos);
				m_kv_accumulator.append(remaining_str);
			}

			if (m_kv_accumulator.length() > MAX_UNPROCESSED_DATA) {
				Delete();
				return;
			}
		}
		end:
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > GP_PING_TIME*2) {
			Delete(true);
		} else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}
	void Peer::handle_packet(std::string packet_string) {
		OS::LogText(OS::ELogLevel_Debug, "[%s] Recv: %s", getAddress().ToString().c_str(), packet_string.c_str());


		std::map<std::string, std::string> data_map;
		data_map["data"] = "length";
		OS::KVReader data_parser = OS::KVReader(packet_string, '\\', 0, data_map);
		
		if(data_parser.Size() == 0) {
			return;
		}
		gettimeofday(&m_last_recv, NULL);

		std::string command = data_parser.GetKeyByIdx(0);
		if(command.compare("auth") == 0) {
			handle_auth(data_parser);
		} else if(command.compare("authp") == 0) {
			handle_authp(data_parser);
		} else if(command.compare("ka") == 0) { //keep alive

		} else if(command.compare("newgame") == 0) {
			handle_newgame(data_parser);
		} else if(command.compare("updgame") == 0) {
			handle_updgame(data_parser);
		} else if(command.compare("getpid") == 0) {
			handle_getpid(data_parser);
		} else if (command.compare("getpd") == 0) {
			handle_getpd(data_parser);
		}

		if(m_user.id != 0) {
			if(command.compare("setpd") == 0) {
				handle_setpd(data_parser);
			}
		}
	}

	void Peer::handle_getpid(OS::KVReader data_parser) {
		/*
				Send error response until implemented
		*/
		int operation_id = data_parser.GetValueInt("lid");
		int pid = data_parser.GetValueInt("pid");
		std::ostringstream ss;
		ss << "\\getpidr\\-1\\lid\\" << operation_id;
		SendPacket(ss.str());
	}
	void Peer::getPersistDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		//// \\getpdr\\1\\lid\\1\\length\\5\\data\\mydata\\final
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;

		std::ostringstream ss;

		uint8_t *data = NULL;
		size_t data_len = 0;
		
		ss << "\\getpdr\\" << success << "\\lid\\" << persist_request_data->operation_id << "\\pid\\" << persist_request_data->profileid << "\\mod\\" << response_data.mod_time;// << "\\length\\" << data_len << "\\data\\";

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
				kv_ss << "\\" << item.first << "\\" << item.second;
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

		peer->SendPacket(buffer);

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
			if(pid != m_profile.id)	{
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
			for (int i = 0; i<keys.length(); i++) {
				if (keys[i] == '\x01') {
					keys[i] = '\\';
				}
			}
			keyList = OS::KeyStringToVector(keys);
		}

		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->profileid = pid;
		persist_request_data->operation_id = operation_id;

		GSBackend::PersistBackendTask::SubmitGetPersistData(pid, this, (void *)persist_request_data, getPersistDataCallback, persist_type, data_index, keyList, modified_since);
	}

	void Peer::setPersistDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;

		std::ostringstream ss;
		ss << "\\setpdr\\" << success << "\\lid\\" << persist_request_data->operation_id << "\\pid\\" << persist_request_data->profileid << "\\mod\\" << response_data.mod_time;
		peer->SendPacket(ss.str());

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

		/* Some games send data-1... but its safe now with the new reader
		if (!data.length() || client_data_len != data.length()) {
			send_error(GPShared::GP_PARSE);
			return;
		}*/

		const char *b64_str = OS::BinToBase64Str((uint8_t *)data.c_str(), client_data_len);

		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->profileid = pid;
		persist_request_data->operation_id = operation_id;


		GSBackend::PersistBackendTask::SubmitSetPersistData(m_profile.id, this, (void *)persist_request_data, setPersistDataCallback, b64_str, persist_type, data_index, kv_set, save_data);

		free((void *)b64_str);
	}

	void Peer::handle_auth(OS::KVReader data_parser) {
		std::string gamename;
		std::string response;
		int local_id = data_parser.GetValueInt("id");
		m_game_port = data_parser.GetValueInt("port");

		if (!data_parser.HasKey("gamename")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		gamename = data_parser.GetValue("gamename");

		if (!data_parser.HasKey("response")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		m_response = data_parser.GetValue("response");


		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->profileid = 0;
		persist_request_data->operation_id = local_id;
		GSBackend::PersistBackendTask::SubmitGetGameInfoByGameName(gamename, this, persist_request_data, onGetGameDataCallback);
	}
	void Peer::onGetGameDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		std::ostringstream ss;
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;
		peer->m_game = response_data.gameData;
		if(peer->m_game.secretkey[0] == 0) {
			peer->send_error(GPShared::GP_CONNECTION_CLOSED);
			return;
		}

		if(!peer->IsResponseValid(peer->m_response.c_str())) {
			peer->send_error(GPShared::GP_CONNECTION_CLOSED);
			peer->Delete();
			return;
		}

		ss << "\\lc\\2\\sesskey\\" << peer->m_session_key << "\\proof\\0\\id\\" << persist_request_data->operation_id;
		peer->SendPacket(ss.str());
	}
	void Peer::newGameCreateCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {

		peer->m_current_game_identifier = response_data.game_instance_identifier;

	}
	void Peer::handle_newgame(OS::KVReader data_parser) {
		GSBackend::PersistBackendTask::SubmitNewGameSession(this, NULL, newGameCreateCallback);
	}
	void Peer::updateGameCreateCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		if (extra) {
			free((void *)extra);
		}
	}
	void Peer::handle_updgame(OS::KVReader data_parser) {
		//\updgame\\sesskey\%d\done\%d\gamedata\%s
		std::map<std::string,std::string> game_data;
		std::string gamedata = data_parser.GetValue("gamedata");

		bool done = data_parser.GetValueInt("done");

		for(int i=0;i<gamedata.length();i++) {
			if(gamedata[i] == '\x1') {
				gamedata[i] = '\\';
			}
		}
		game_data = OS::KeyStringToMap(gamedata);
		GSBackend::PersistBackendTask::SubmitUpdateGameSession(game_data, this, NULL, m_current_game_identifier, updateGameCreateCallback, done);
	}

	void Peer::perform_pid_auth(int profileid, const char *response, int operation_id) {
		OS::AuthTask::TryAuthPID_GStatsSessKey(profileid, m_session_key, response, m_nick_email_auth_cb, NULL, operation_id, this);
	}
	void Peer::perform_preauth_auth(std::string auth_token, const char *response, int operation_id) {
		OS::AuthTask::TryAuth_PreAuth_GStatsSessKey(m_session_key, response, auth_token, m_nick_email_auth_cb, NULL, operation_id, this);
	}
	void Peer::handle_authp(OS::KVReader data_parser) {
		// TODO: CD KEY AUTH
		int pid = data_parser.GetValueInt("pid");

		int operation_id = data_parser.GetValueInt("lid");

		std::string resp;
		if (!data_parser.HasKey("resp")) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		resp = data_parser.GetValue("resp");

		if(pid != 0) {
			perform_pid_auth(pid, resp.c_str(), operation_id);
		} else {
			if (data_parser.HasKey("authtoken")) {
				std::string auth_token = data_parser.GetValue("authtoken");
				perform_preauth_auth(auth_token, resp.c_str(), operation_id);
				return;
			}
			send_error(GPShared::GP_PARSE);
		}

	}
	void Peer::m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id, INetPeer *peer) {

		if(!((Peer *)peer)->m_backend_session_key.length() && auth_data.session_key.length())
			((Peer *)peer)->m_backend_session_key = auth_data.session_key;

		((Peer *)peer)->m_user = user;
		((Peer *)peer)->m_profile = profile;

		std::ostringstream ss;


		if(success) {
			ss << "\\pauthr\\" << profile.id;

			if(auth_data.session_key.length()) {
				ss << "\\lt\\" << auth_data.session_key;
			}
			ss << "\\lid\\" << operation_id;

			((Peer *)peer)->m_profile = profile;
			((Peer *)peer)->m_user = user;

			((Peer *)peer)->SendPacket(ss.str());

		} else {
			ss << "\\pauthr\\-1\\errmsg\\";
			GPShared::GPErrorData error_data;
			GPShared::GPErrorCode code;

			switch(auth_data.response_code) {
				case OS::LOGIN_RESPONSE_USER_NOT_FOUND:
					//code = GP_LOGIN_BAD_EMAIL;
					code = GP_LOGIN_BAD_PROFILE;
					break;
				case OS::LOGIN_RESPONSE_INVALID_PASSWORD:
					code = GP_LOGIN_BAD_PASSWORD;
				break;
				case OS::LOGIN_RESPONSE_INVALID_PROFILE:
					code = GP_LOGIN_BAD_PROFILE;
				break;
				case OS::LOGIN_RESPONSE_UNIQUE_NICK_EXPIRED:
					code = GP_LOGIN_BAD_UNIQUENICK;
				break;
				case OS::LOGIN_RESPONSE_DB_ERROR:
					code = GP_DATABASE;
				break;
				case OS::LOGIN_RESPONSE_SERVERINITFAILED:
				case OS::LOGIN_RESPONSE_SERVER_ERROR:
					code = GP_NETWORK;
			}

			error_data = GPShared::getErrorDataByCode(code);

			ss << error_data.msg;

			ss << "\\lid\\" << operation_id;
			((Peer *)peer)->SendPacket(ss.str());
		}
	}

	void Peer::SendPacket(std::string str, bool attach_final) {
		OS::Buffer buffer;
		buffer.WriteBuffer((void *)str.c_str(), str.length());
		SendPacket(buffer, attach_final);
	}
	void Peer::SendPacket(OS::Buffer &buffer, bool attach_final) {
		if(attach_final) {
			buffer.WriteBuffer((uint8_t*)"\\final\\", 7);
		}
		OS::LogText(OS::ELogLevel_Debug, "[%s] Send: %s", getAddress().ToString().c_str(), std::string((const char *)buffer.GetHead(), buffer.size()).c_str());
		gamespy3dxor((char *)buffer.GetHead(), buffer.size());

		NetIOCommResp io_resp;
		io_resp = this->GetDriver()->getServer()->getNetIOInterface()->streamSend(m_sd, buffer);
		if (io_resp.disconnect_flag || io_resp.error_flag) {
			Delete();
		}
	}
	void Peer::send_ping() {
		//check for timeout
		struct timeval current_time;

		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_ping.tv_sec > GP_PING_TIME) {
			gettimeofday(&m_last_ping, NULL);
			//std::string ping_packet = "\\ka\\";
			//SendPacket((const uint8_t *)ping_packet.c_str(),ping_packet.length());
		}
	}

	void Peer::send_error(GPShared::GPErrorCode code) {
		GPShared::GPErrorData error_data = GPShared::getErrorDataByCode(code);
		if(error_data.msg == NULL) {
			m_delete_flag = true;
			return;
		}
		std::ostringstream ss;
		ss << "\\error\\";
		ss << "\\err\\" << error_data.error;
		ss << "\\errmsg\\" << error_data.msg;
		if(error_data.die) {
			ss << "\\fatal\\" << error_data.die;
		}
		SendPacket(ss.str());
		if (error_data.die) {
			Delete();
		}
	}

	void Peer::gamespy3dxor(char *data, size_t len) {
	    static const char gamespy[] = "GameSpy3D\0";
	    char  *gs;

	    gs = (char *)gamespy;
	    while(len-- && len >= 0) {
			if(strncmp(data,"\\final\\",7) == 0)  {
				data+=7;
				len-=7;
				gs = (char *)gamespy;
				continue;
			}
	        *data++ ^= *gs++;
	        if(!*gs) gs = (char *)gamespy;
	    }
	}
	bool Peer::IsResponseValid(const char *response) {
		int chrespnum = gs_chresp_num(m_challenge);
		std::ostringstream s;
		s << chrespnum << m_game.secretkey;

		bool ret = false;

		const char *true_resp = OS::MD5String(s.str().c_str());

		ret = strcmp(response,true_resp) == 0;

		free((void *)true_resp);

		return ret;

	}
	int Peer::gs_chresp_num(const char *challenge) {
	    int     num = 0;

	    while(*challenge) {
	        num = *challenge - (num * 0x63306CE7);
	        challenge++;
	    }
	    return(num);
	}
	void Peer::gs_sesskey(int sesskey, char *out) {
	    int         i = 17;
		char 		*p;

	    sprintf(out, "%.8x", sesskey ^ 0x38f371e6);
	    for(p = out; *p; p++, i++) {
	        *p += i;
	    }
	}

	int Peer::GetProfileID() {
		return m_profile.id;
	}

	void Peer::Delete(bool timeout) {
		m_delete_flag = true;
		m_timeout_flag = timeout;
	}
}
