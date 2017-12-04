#include "GSPeer.h"
#include "GSDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>

#include <OS/Search/User.h>

#include <sstream>
#include <algorithm>

#include "GSBackend.h"

#include <OS/GPShared.h>

#include <map>
#include <utility>

using namespace GPShared;

namespace GS {

	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) : INetPeer(driver, address_info, sd) {
		ResetMetrics();
		m_sd = sd;
		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		mp_mutex = OS::CreateMutex();
		gettimeofday(&m_last_ping, NULL);

		m_user.id = 0;
		m_profile.id = 0;

		m_session_key = 1234567;

		gen_random(m_challenge, CHALLENGE_LEN);

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection",OS::Address(m_address_info).ToString().c_str());

		send_login_challenge(1);
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed",OS::Address(m_address_info).ToString().c_str());
		delete mp_mutex;
		close(m_sd);
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
		SendPacket((const uint8_t *)s.str().c_str(),s.str().length());
	}
	void Peer::think(bool packet_waiting) {
		char buf[GPI_READ_SIZE + 1];
		socklen_t slen = sizeof(struct sockaddr_in);
		int len, piece_len;
		if (packet_waiting) {
			len = recv(m_sd, (char *)&buf, GPI_READ_SIZE, 0);
			if (OS::wouldBlock()) {
				return;
			}
			if(len <= 0) goto end;
			buf[len] = 0;

			gamespy3dxor((char *)&buf, len);

			/* split by \\final\\  */
			char *p = (char *)&buf;
			char *x;
			while(true) {
				x = p;
				p = strstr(p,"\\final\\");
				if(p == NULL) { break; }
				*p = 0;
				piece_len = strlen(x);
				this->handle_packet(x, piece_len);
				p+=7; //skip final
			}

			piece_len = strlen(x);
			if(piece_len > 0) {
				this->handle_packet(x, piece_len);
			}			
		}

		end:
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if(current_time.tv_sec - m_last_recv.tv_sec > GP_PING_TIME*2) {
			m_delete_flag = true;
			m_timeout_flag = true;
		} 
		if (len <= 0 && packet_waiting) {
			m_delete_flag = true;
		}
	}
	void Peer::handle_packet(char *data, int len) {
		printf("GStats Handle(%d): %s\n", len,data);

		OS::KVReader data_parser = OS::KVReader(std::string(data));
		std::string command;
		memset(&command,0,sizeof(command));
		if(data_parser.Size() == 0) {
			m_delete_flag = true;
			return;
		}
		if(len > 0)
			gettimeofday(&m_last_recv, NULL);

		command = data_parser.GetKeyByIdx(0);
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
		SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
	}
	void Peer::getPersistDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		//// \\getpdr\\1\\lid\\1\\length\\5\\data\\mydata\\final
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;
		printf("Got persist data: %s\n", response_data.game_instance_identifier.c_str());

		std::ostringstream ss;
		//void Base64StrToBin(const char *str, uint8_t **out, int &len);
		uint8_t *data;
		int data_len = 0;
		OS::Base64StrToBin(response_data.game_instance_identifier.c_str(), &data, data_len);
		ss << "\\getpdr\\" << 1 << "\\lid\\" << persist_request_data->operation_id << "\\pid\\" << persist_request_data->profileid << "\\length\\" << data_len << "\\data\\";

		//SendPacket
		uint8_t out_buff[GPI_READ_SIZE + 1];
		memset(&out_buff, 0, sizeof(out_buff));
		int out_len = 0;
		uint8_t *p = (uint8_t *)(&out_buff);
		BufferWriteData(&p, &out_len, (uint8_t *)ss.str().c_str(), ss.str().length());

		BufferWriteData(&p, &out_len, (uint8_t *)data, data_len);

		peer->SendPacket((const uint8_t *)&out_buff,out_len);

		free((void *)data);
		free((void *)persist_request_data);
	}
	void Peer::handle_getpd(OS::KVReader data_parser) {
		int operation_id = data_parser.GetValueInt("lid");
		int pid = data_parser.GetValueInt("pid");
		int data_index = data_parser.GetValueInt("dindex");

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

		GSBackend::PersistBackendTask::SubmitGetPersistData(pid, this, (void *)persist_request_data, getPersistDataCallback, persist_type, data_index, keyList);
	}

	void Peer::setPersistDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;

		std::ostringstream ss;
		ss << "\\setpdr\\" << success << "\\lid\\" << persist_request_data->operation_id << "\\pid\\" << persist_request_data->profileid;
		peer->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());

		free((void *)persist_request_data);
	}

	void Peer::handle_setpd(OS::KVReader data_parser) {
		int operation_id = data_parser.GetValueInt("lid");
		int pid = data_parser.GetValueInt("pid");
		int data_index = data_parser.GetValueInt("dindex");

		persisttype_t persist_type = (persisttype_t)data_parser.GetValueInt("ptype");


		if(pid != m_profile.id) {
			send_error(GPShared::GP_NOT_LOGGED_IN);
			return;
		}

		bool kv_set = data_parser.GetValueInt("kv") != 0;

		std::string data = data_parser.GetValue("data");
		int client_data_len = data_parser.GetValueInt("length");
		if(!data.length() || client_data_len != data.length()) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		const char *b64_str = OS::BinToBase64Str((uint8_t *)data.c_str(), client_data_len);

		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->profileid = pid;
		persist_request_data->operation_id = operation_id;

		GSBackend::PersistBackendTask::SubmitSetPersistData(m_profile.id, this, (void *)persist_request_data, setPersistDataCallback, b64_str, persist_type, data_index, kv_set);

		free((void *)b64_str);
	}

	void Peer::handle_auth(OS::KVReader data_parser) {
		std::string gamename;
		std::string response;
		int local_id = data_parser.GetValueInt("id");
		m_game_port = data_parser.GetValueInt("port");

		std::ostringstream ss;

		if (!data_parser.HasKey("gamename")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		gamename = data_parser.GetValue("gamename");

		if (!data_parser.HasKey("response")) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		response = data_parser.GetValue("response");

		m_game = OS::GetGameByName(gamename.c_str());
		if(m_game.secretkey[0] == 0) {
			send_error(GPShared::GP_CONNECTION_CLOSED);
			return;
		}

		if(!IsResponseValid(response.c_str())) {
			send_error(GPShared::GP_CONNECTION_CLOSED);
			m_delete_flag = true;
			return;
		}

		ss << "\\lc\\2\\sesskey\\" << m_session_key << "\\proof\\0\\id\\" << local_id;
		SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
	}
	void Peer::newGameCreateCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {

		peer->m_current_game_identifier = response_data.game_instance_identifier;

	}
	void Peer::handle_newgame(OS::KVReader data_parser) {
		GSBackend::PersistBackendTask::SubmitNewGameSession(this, NULL, newGameCreateCallback);
	}
	void Peer::updateGameCreateCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {

	}
	void Peer::handle_updgame(OS::KVReader data_parser) {
		//\updgame\\sesskey\%d\done\%d\gamedata\%s
		std::map<std::string,std::string> game_data;
		std::string gamedata = data_parser.GetValue("gamedata");

		if (data_parser.HasKey("gamedata")) {

		}
		for(int i=0;i<gamedata.length();i++) {
			if(gamedata[i] == '\x1') {
				gamedata[i] = '\\';
			}
		}
		game_data = OS::KeyStringToMap(gamedata);
		GSBackend::PersistBackendTask::SubmitUpdateGameSession(game_data, this, NULL, m_current_game_identifier, updateGameCreateCallback);
	}

	void Peer::perform_pid_auth(int profileid, const char *response, int operation_id) {
 		OS::AuthTask::TryAuthPID_GStatsSessKey(profileid, m_session_key, response, m_nick_email_auth_cb, this, operation_id);
	}
	void Peer::handle_authp(OS::KVReader data_parser) {
		// TODO: CD KEY AUTH
		int pid = data_parser.GetValueInt("pid");

		int operation_id = data_parser.GetValueInt("lid");

		std::string resp;
		if (data_parser.HasKey("resp")) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		resp = data_parser.GetValue("resp");

		if(pid != 0) {
			perform_pid_auth(pid, resp.c_str(), operation_id);
		} else {
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

			((Peer *)peer)->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());

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
			((Peer *)peer)->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
		}
	}


	void Peer::SendPacket(const uint8_t *buff, int len, bool attach_final) {
		uint8_t out_buff[GPI_READ_SIZE + 1];
		uint8_t *p = (uint8_t*)&out_buff;
		int out_len = 0;
		BufferWriteData(&p, &out_len, buff, len);

		printf("Sending: %s \n", buff);
		if(attach_final) {
			BufferWriteData(&p, &out_len, (uint8_t*)"\\final\\", 7);
		}
		gamespy3dxor((char *)&out_buff, out_len);
		int c = send(m_sd, (const char *)&out_buff, out_len, MSG_NOSIGNAL);

		if(c < 0) {
			m_delete_flag = true;
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
			m_delete_flag = true;
		}
		printf("Send error: %s\n", error_data.msg);
		SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
	}

	void Peer::gamespy3dxor(char *data, int len) {
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

	OS::MetricValue Peer::GetMetricItemFromStats(PeerStats stats) {
		OS::MetricValue arr_value, value;
		value.value._str = stats.m_address.ToString(false);
		value.key = "ip";
		value.type = OS::MetricType_String;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.disconnected;
		value.key = "disconnected";
		value.type = OS::MetricType_Integer;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.type = OS::MetricType_Integer;
		value.value._int = stats.bytes_in;
		value.key = "bytes_in";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.bytes_out;
		value.key = "bytes_out";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.packets_in;
		value.key = "packets_in";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.packets_out;
		value.key = "packets_out";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.value._int = stats.total_requests;
		value.key = "pending_requests";
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));
		arr_value.type = OS::MetricType_Array;

		value.type = OS::MetricType_String;
		value.key = "gamename";
		value.value._str = stats.from_game.gamename;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		value.type = OS::MetricType_Integer;
		value.key = "version";
		value.value._int = stats.version;
		arr_value.arr_value.values.push_back(std::pair<OS::MetricType, struct OS::_Value>(OS::MetricType_String, value));

		arr_value.key = stats.m_address.ToString(false);
		return arr_value;
	}
	void Peer::ResetMetrics() {
		m_peer_stats.bytes_in = 0;
		m_peer_stats.bytes_out = 0;
		m_peer_stats.packets_in = 0;
		m_peer_stats.packets_out = 0;
		m_peer_stats.total_requests = 0;
	}
	OS::MetricInstance Peer::GetMetrics() {
		OS::MetricInstance peer_metric;

		peer_metric.value = GetMetricItemFromStats(m_peer_stats);
		peer_metric.key = "peer";

		ResetMetrics();

		return peer_metric;
	}
}
