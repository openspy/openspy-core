#include "GPPeer.h"
#include "GPDriver.h"
#include <OS/OpenSpy.h>
#include <OS/Search/Profile.h>
#include <OS/legacy/helpers.h>
#include <OS/legacy/buffreader.h>
#include <OS/legacy/buffwriter.h>
#include <OS/socketlib/socketlib.h>

#include <OS/Search/User.h>

#include <sstream>
#include <algorithm>

#include "GPBackend.h"

#include <OS/GPShared.h>

using namespace GPShared;

namespace GP {

	Peer::Peer(Driver *driver, struct sockaddr_in *address_info, int sd) {
		m_sd = sd;
		mp_driver = driver;
		m_address_info = *address_info;
		m_delete_flag = false;
		m_timeout_flag = false;
		mp_mutex = OS::CreateMutex();
		gettimeofday(&m_last_ping, NULL);

		m_session_key = 0;

		gen_random(m_challenge, CHALLENGE_LEN);

		send_login_challenge(1);

		//formatSend(sd,true,2,"\\lc\\1\\challenge\\%s\\id\\1",challenge);
	}
	Peer::~Peer() {
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
		} else if(len == 0 && packet_waiting) {
			m_delete_flag = true;
		}
	}
	void Peer::handle_packet(char *data, int len) {
		printf("GStats Handle(%d): %s\n", len,data);
		char command[32];
		memset(&command,0,sizeof(command));
		if(!find_param(0, data,(char *)&command, sizeof(command))) {
			m_delete_flag = true;
			return;
		}
		gettimeofday(&m_last_recv, NULL);

		printf("GStats Got cmd: %s\n", command);

		if(strcmp(command, "auth") == 0) {
			handle_auth(data, len);
		} else if(strcmp(command, "ka") == 0) { //keep alive

		}
	}

	void Peer::handle_auth(const char *data, int len) {
		char gamename[32 + 1];
		char response[32 + 1];
		int local_id = find_paramint("id", (char *)data);
		int port = find_paramint("port", (char *)data);

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
		if(error_data.die)
			ss << "\\fatal\\" << error_data.die;
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


	int Peer::GetProfileID() {
		return m_profile.id;
	}
}