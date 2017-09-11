#include "GSPeer.h"
#include "GSDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>

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

		OS::LogText(OS::ELogLevel_Info, "New connection from %s",OS::Address(m_address_info).ToString().c_str());

		send_login_challenge(1);

	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "Connection from %s closed",OS::Address(m_address_info).ToString().c_str());
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
			buf[len] = 0;
			if(len == 0) goto end;

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
	}
	void Peer::handle_packet(char *data, int len) {
		//printf("GStats Handle(%d): %s\n", len,data);
		char command[32];
		memset(&command,0,sizeof(command));
		if(!find_param(0, data,(char *)&command, sizeof(command))) {
			m_delete_flag = true;
			return;
		}
		if(len > 0)
			gettimeofday(&m_last_recv, NULL);

		if(strcmp(command, "auth") == 0) {
			handle_auth(data, len);
		} else if(strcmp(command, "authp") == 0) {
			handle_authp(data, len);
		} else if(strcmp(command, "ka") == 0) { //keep alive

		} else if(strcmp(command, "newgame") == 0) {
			handle_newgame(data, len);
		} else if(strcmp(command, "updgame") == 0) {
			handle_updgame(data, len);
		} else if(strcmp(command, "getpid") == 0) {
			handle_getpid(data, len);
		}

		if(m_user.id != 0) {
			if(strcmp(command, "setpd") == 0) {
				handle_setpd(data, len);
			} else if(strcmp(command, "getpd") == 0) {
				handle_getpd(data, len); //getpd might be OK for public data	to work when not logged in
			}
		}
	}

	void Peer::handle_getpid(const char *data, int len) {
		/*
				Send error response until implemented
		*/
		int operation_id = find_paramint("lid", (char *)data);
		int pid = find_paramint("pid", (char *)data);
		std::ostringstream ss;
		ss << "\\getpidr\\-1\\lid\\" << operation_id;
		SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
	}
	void Peer::getPersistDataCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		if(!g_gbl_gp_driver->HasPeer(peer)) {
			free((void *)extra);
			return;
		}
		//// \\getpdr\\1\\lid\\1\\length\\5\\data\\mydata\\final
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;
		printf("Got persist data: %s\n", response_data.game_instance_identifier.c_str());

		std::ostringstream ss;
		//void Base64StrToBin(const char *str, uint8_t **out, int &len);
		uint8_t *data;
		int data_len = 0;
		OS::Base64StrToBin(response_data.game_instance_identifier.c_str(), &data, data_len);
		ss << "\\getpdr\\" << success << "\\lid\\" << persist_request_data->operation_id << "\\pid\\" << persist_request_data->profileid << "\\length\\" << data_len << "\\data\\";

		//SendPacket
		uint8_t out_buff[GPI_READ_SIZE + 1];
		int out_len = 0;
		uint8_t *p = (uint8_t *)(&out_buff);
		BufferWriteData(&p, &out_len, (uint8_t *)ss.str().c_str(), ss.str().length());

		BufferWriteData(&p, &out_len, (uint8_t *)data, data_len);

		peer->SendPacket((const uint8_t *)&out_buff,out_len);

		free((void *)data);
		free((void *)persist_request_data);
	}
	void Peer::handle_getpd(const char *data, int len) {
		int operation_id = find_paramint("lid", (char *)data);
		int pid = find_paramint("pid", (char *)data);
		int data_index = find_paramint("dindex", (char *)data);

		persisttype_t persist_type = (persisttype_t)find_paramint("ptype", (char *)data);

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

		char keys[1000];
		std::vector<std::string> keyList;
		if(find_param("keys", (char *)data, keys, sizeof(keys))) {
			for(int i=0;i<sizeof(keys);i++) {
				if(keys[i] == '\x01') {
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
		if(!g_gbl_gp_driver->HasPeer(peer)) {
			free((void *)extra);
			return;
		}
		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)extra;

		std::ostringstream ss;
		ss << "\\setpdr\\" << success << "\\lid\\" << persist_request_data->operation_id << "\\pid\\" << persist_request_data->profileid;
		peer->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());

		free((void *)persist_request_data);
	}

	void Peer::handle_setpd(const char *data, int len) {
		int operation_id = find_paramint("lid", (char *)data);
		int pid = find_paramint("pid", (char *)data);
		int data_index = find_paramint("dindex", (char *)data);

		persisttype_t persist_type = (persisttype_t)find_paramint("ptype", (char *)data);


		if(pid != m_profile.id) {
			send_error(GPShared::GP_NOT_LOGGED_IN);
			return;
		}

		bool kv_set = find_paramint("kv", (char *)data) != 0;

		char *data_pos = strstr((char *)data, "\\data\\");
		int client_data_len = find_paramint("length", (char *)data);
		if(client_data_len > len || !data_pos) {
			send_error(GPShared::GP_PARSE);
			return;
		} else {
			data_pos += 6; //strlen of data key
		}

		const char *b64_str = OS::BinToBase64Str((uint8_t *)data_pos, client_data_len);

		GPPersistRequestData *persist_request_data = (GPPersistRequestData *)malloc(sizeof(GPPersistRequestData));
		persist_request_data->profileid = pid;
		persist_request_data->operation_id = operation_id;

		GSBackend::PersistBackendTask::SubmitSetPersistData(m_profile.id, this, (void *)persist_request_data, setPersistDataCallback, b64_str, persist_type, data_index, kv_set);

		free((void *)b64_str);
	}

	void Peer::handle_auth(const char *data, int len) {
		char gamename[32 + 1];
		char response[32 + 1];
		int local_id = find_paramint("id", (char *)data);
		m_game_port = find_paramint("port", (char *)data);

		std::ostringstream ss;

		if(!find_param("gamename", (char *)data, gamename, sizeof(gamename))) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		if(!find_param("response", (char *)data, response, sizeof(response))) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		m_game = OS::GetGameByName(gamename);
		if(m_game.secretkey[0] == 0) {
			send_error(GPShared::GP_CONNECTION_CLOSED);
			return;
		}

		if(!IsResponseValid(response)) {
			send_error(GPShared::GP_CONNECTION_CLOSED);
			m_delete_flag = true;
			return;
		}

		ss << "\\lc\\2\\sesskey\\" << m_session_key << "\\proof\\0\\id\\" << local_id;
		SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
	}
	void Peer::newGameCreateCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		if(!g_gbl_gp_driver->HasPeer(peer))
			return;

		peer->m_current_game_identifier = response_data.game_instance_identifier;

	}
	void Peer::handle_newgame(const char *data, int len) {
		GSBackend::PersistBackendTask::SubmitNewGameSession(this, NULL, newGameCreateCallback);
	}
	void Peer::updateGameCreateCallback(bool success, GSBackend::PersistBackendResponse response_data, GS::Peer *peer, void* extra) {
		if(!g_gbl_gp_driver->HasPeer(peer))
			return;
	}
	void Peer::handle_updgame(const char *data, int len) {
		//\updgame\\sesskey\%d\done\%d\gamedata\%s
		std::map<std::string,std::string> game_data;
		char gamedata[1000];
		memset(&gamedata, 0, sizeof(gamedata));
		if(!find_param("gamedata", (char *)data, gamedata, sizeof(gamedata)-1)) {
			send_error(GPShared::GP_PARSE);
			return;
		}
		for(int i=0;i<sizeof(gamedata);i++) {
			if(gamedata[i] == '\x1') {
				gamedata[i] = '\\';
			}
		}
		game_data = OS::KeyStringToMap(gamedata);
		GSBackend::PersistBackendTask::SubmitUpdateGameSession(game_data, this, NULL, m_current_game_identifier, updateGameCreateCallback);
		/*
		std::map<std::string,std::string>::iterator it = game_data.begin();
		while(it != game_data.end()) {
			std::pair<std::string,std::string> p = *it;
			printf("Game Data: %s - %s\n",p.first.c_str(),p.second.c_str());
			it++;
		}
		*/

	}

	void Peer::perform_pid_auth(int profileid, const char *response, int operation_id) {
 		OS::AuthTask::TryAuthPID_GStatsSessKey(profileid, m_session_key, response, m_nick_email_auth_cb, this, operation_id);
	}
	void Peer::handle_authp(const char *data, int len) {
		// TODO: CD KEY AUTH
		char response[64];
		memset(&response,0,sizeof(response));
		int pid = find_paramint("pid", (char *)data);

		int operation_id = find_paramint("lid", (char *)data);

		if(!find_param("resp", (char *)data, response, sizeof(response)-1)) {
			send_error(GPShared::GP_PARSE);
			return;
		}

		if(pid != 0) {
			perform_pid_auth(pid, response, operation_id);
		} else {
			send_error(GPShared::GP_PARSE);
		}

	}
	void Peer::m_nick_email_auth_cb(bool success, OS::User user, OS::Profile profile, OS::AuthData auth_data, void *extra, int operation_id) {
		Peer *peer = (Peer *)extra;
		if(!g_gbl_gp_driver->HasPeer(peer)) {
			return;
		}

		if(!peer->m_backend_session_key.length() && auth_data.session_key)
			peer->m_backend_session_key = auth_data.session_key;

		peer->m_user = user;
		peer->m_profile = profile;

		std::ostringstream ss;


		if(success) {
			ss << "\\pauthr\\" << profile.id;

			if(auth_data.session_key) {
				ss << "\\lt\\" << auth_data.session_key;
			}
			ss << "\\lid\\" << operation_id;

			peer->m_profile = profile;
			peer->m_user = user;

			peer->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());

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
			peer->SendPacket((const uint8_t *)ss.str().c_str(),ss.str().length());
		}
	}


	void Peer::SendPacket(const uint8_t *buff, int len, bool attach_final) {
		uint8_t out_buff[GPI_READ_SIZE + 1];
		uint8_t *p = (uint8_t*)&out_buff;
		int out_len = 0;
		BufferWriteData(&p, &out_len, buff, len);
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
			std::string ping_packet = "\\ka\\";
			SendPacket((const uint8_t *)ping_packet.c_str(),ping_packet.length());
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
}