#include <OS/OpenSpy.h>

#include <OS/SharedTasks/tasks.h>
#include "FESLServer.h"
#include "FESLDriver.h"
#include "FESLPeer.h"


#include <sstream>

namespace FESL {
	CommandHandler Peer::m_commands[] = {
		{ FESL_TYPE_FSYS, "Hello", &Peer::m_fsys_hello_handler },
		{ FESL_TYPE_FSYS, "Ping", &Peer::m_fsys_ping_handler },
		{ FESL_TYPE_FSYS, "MemCheck", &Peer::m_fsys_memcheck_handler },
		{ FESL_TYPE_FSYS, "Goodbye", &Peer::m_fsys_goodbye_handler },
		{ FESL_TYPE_SUBS, "GetEntitlementByBundle", &Peer::m_subs_get_entitlement_by_bundle },
		{ FESL_TYPE_DOBJ, "GetObjectInventory", &Peer::m_dobj_get_object_inventory },
		{ FESL_TYPE_ACCOUNT, "Login", &Peer::m_acct_login_handler },
		{ FESL_TYPE_ACCOUNT, "NuLogin", &Peer::m_acct_nulogin_handler },
		{ FESL_TYPE_ACCOUNT, "NuGetPersonas", &Peer::m_acct_get_personas},
		{ FESL_TYPE_ACCOUNT, "NuLoginPersona",  &Peer::m_acct_login_persona },
		{ FESL_TYPE_ACCOUNT, "GetTelemetryToken",  &Peer::m_acct_get_telemetry_token},
		{ FESL_TYPE_ACCOUNT, "RegisterGame", &Peer::m_acct_register_game_handler },
		{ FESL_TYPE_ACCOUNT, "GetCountryList", &Peer::m_acct_get_country_list },
		{ FESL_TYPE_ACCOUNT, "GetTos", &Peer::m_acct_gettos_handler },
		{ FESL_TYPE_ACCOUNT, "GetSubAccounts", &Peer::m_acct_get_sub_accounts },
		{ FESL_TYPE_ACCOUNT, "LoginSubAccount",  &Peer::m_acct_login_sub_account },
		{ FESL_TYPE_ACCOUNT, "AddSubAccount",  &Peer::m_acct_add_sub_account },
		{ FESL_TYPE_ACCOUNT, "AddAccount",  &Peer::m_acct_add_account },
		{ FESL_TYPE_ACCOUNT, "UpdateAccount",  &Peer::m_acct_update_account },
		{ FESL_TYPE_ACCOUNT, "DisableSubAccount",  &Peer::m_acct_disable_sub_account },
		{ FESL_TYPE_ACCOUNT, "GetAccount", &Peer::m_acct_get_account },
		{ FESL_TYPE_ACCOUNT, "GameSpyPreAuth", &Peer::m_acct_gamespy_preauth },
		{ FESL_TYPE_ACCOUNT, "SendAccountName", &Peer::m_acct_send_account_name},
		{ FESL_TYPE_ACCOUNT, "SendPassword", &Peer::m_acct_send_account_password},
	};
	Peer::Peer(Driver *driver, INetIOSocket *sd) : INetPeer(driver, sd) {
		m_delete_flag = false;
		m_timeout_flag = false;

		gettimeofday(&m_last_ping, NULL);
		gettimeofday(&m_last_recv, NULL);
		
		m_sequence_id = 1;
		m_logged_in = false;
		m_pending_subaccounts = false;
		m_got_profiles = false;
		m_pending_nuget_personas = false;

		mp_mutex = OS::CreateMutex();

		OS::LogText(OS::ELogLevel_Info, "[%s] New connection", m_sd->address.ToString().c_str());
	}
	Peer::~Peer() {
		OS::LogText(OS::ELogLevel_Info, "[%s] Connection closed, timeout: %d", m_sd->address.ToString().c_str(), m_timeout_flag);
		delete mp_mutex;
	}
	void Peer::think(bool packet_waiting) {
		NetIOCommResp io_resp;
		if (m_delete_flag) return;
		if (packet_waiting) {
			OS::Buffer recv_buffer;

			io_resp = ((FESL::Driver *)GetDriver())->getSSL_Socket_Interface()->streamRecv(m_sd, recv_buffer);

			int len = io_resp.comm_len;

			if (len <= 0) {
				goto end;
			}

			FESL_HEADER header;
			recv_buffer.ReadBuffer(&header, sizeof(header));

			gettimeofday(&m_last_recv, NULL);

			int buf_len = len - sizeof(FESL_HEADER);

			if (buf_len < 0) {
				goto end;
			}

			OS::KVReader kv_data(std::string((const char *)recv_buffer.GetReadCursor(), buf_len), '=', '\n');
			char *type;
			for (size_t i = 0; i < sizeof(m_commands) / sizeof(CommandHandler); i++) {
				if (Peer::m_commands[i].type == ntohl(header.type)) {
					if (Peer::m_commands[i].command.compare(kv_data.GetValue("TXN")) == 0) {
						type = (char *)&Peer::m_commands[i].type;
						OS::LogText(OS::ELogLevel_Info, "[%s] Got Command: %c%c%c%c %s", m_sd->address.ToString().c_str(), type[3], type[2], type[1], type[0], Peer::m_commands[i].command.c_str());
						(*this.*Peer::m_commands[i].mpFunc)(kv_data);
						return;
					}
				}
			}
			header.type = ntohl(header.type);
			type = (char *)&header.type;
			OS::LogText(OS::ELogLevel_Info, "[%s] Got Unknown Command: %c%c%c%c %s", m_sd->address.ToString().c_str(), type[3], type[2], type[1], type[0], kv_data.GetValue("TXN").c_str());
		}

		end:
		send_ping();

		//check for timeout
		struct timeval current_time;
		gettimeofday(&current_time, NULL);
		if (current_time.tv_sec - m_last_recv.tv_sec > FESL_PING_TIME * 2) {
			Delete(true);
		}
		else if ((io_resp.disconnect_flag || io_resp.error_flag) && packet_waiting) {
			Delete();
		}
	}
	void Peer::SendPacket(FESL_COMMAND_TYPE type, std::string data, int force_sequence) {
		FESL_HEADER header;
		if (force_sequence == -1) {
			header.subtype = htonl(0x80000000 | m_sequence_id++);
		}
		else {
			header.subtype = htonl(0x80000000 | force_sequence);
		}
		
		header.type = htonl(type);
		header.len = htonl((u_long)data.length() + sizeof(header) + 1);

		OS::Buffer send_buf;
		send_buf.WriteBuffer((void *)&header, sizeof(header));
		send_buf.WriteNTS(data);

		NetIOCommResp io_resp = ((FESL::Driver *)GetDriver())->getSSL_Socket_Interface()->streamSend(m_sd, send_buf);

		if (io_resp.disconnect_flag || io_resp.error_flag)
			Delete();

	}
	void Peer::m_search_callback(TaskShared::WebErrorDetails error_details, std::vector<OS::Profile> results, std::map<int, OS::User> result_users, void *extra, INetPeer *peer) {
		((Peer *)peer)->mp_mutex->lock();
		((Peer *)peer)->m_profiles = results;
		((Peer *)peer)->mp_mutex->unlock();
		((Peer *)peer)->m_got_profiles = true;
		if (((Peer *)peer)->m_pending_subaccounts) {
			((Peer *)peer)->m_pending_subaccounts = false;
			((Peer *)peer)->send_subaccounts();
		}

		if (((Peer *)peer)->m_pending_nuget_personas) {
			((Peer *)peer)->m_pending_nuget_personas = false;
			((Peer *)peer)->send_personas();
		}
	}

	void Peer::send_memcheck(int type, int salt) {
		std::ostringstream s;
		s << "TXN=MemCheck\n";
		s << "memcheck.[]=0\n";
		s << "type=" << type << "\n";
		s << "salt=" << time(NULL) <<"\n";
		SendPacket(FESL_TYPE_FSYS, s.str(), 0);
	}

	void Peer::SendCustomError(FESL_COMMAND_TYPE type, std::string TXN, std::string fieldName, std::string fieldError) {
		std::ostringstream s;
		s << "TXN=" << TXN << "\n";
		s << "errorContainer=[]\n";
		s << "errorCode=" << FESL_ERROR_CUSTOM << "\n";
		s << "errorContainer.0.fieldName=" << fieldName << "\n";
		s << "errorContainer.0.fieldError=" << fieldError << "\n";
		SendPacket(type, s.str());
	}
	void Peer::SendError(FESL_COMMAND_TYPE type, FESL_ERROR error, std::string TXN) {
		std::ostringstream s;
		s << "TXN=" << TXN << "\n";
		s << "errorContainer=[]\n";
		if (error == (FESL_ERROR)0) {
			s << "errorType=" << error << "\n";
		}
		s << "errorCode=" << error << "\n";
		SendPacket(type, s.str());
	}

	void Peer::Delete(bool timeout) {
		m_timeout_flag = timeout;
		m_delete_flag = true;
	}
	bool Peer::GetAuthCredentials(OS::User &user, OS::Profile &profile) {
		if(m_user.id) {
			user = m_user;
			profile = m_profile;
			return true;
		}
		return false;
	}
	void Peer::DuplicateLoginExit() {
		std::string kv_str = "TXN=Goodbye\n";
		SendPacket(FESL_TYPE_FSYS, kv_str);
		Delete();
	}

	void Peer::handle_auth_callback_error(TaskShared::WebErrorDetails error_details, FESL_COMMAND_TYPE cmd_type, std::string TXN) {
		FESL_ERROR error = FESL_ERROR_AUTH_FAILURE;
		switch (error_details.response_code) {
		case TaskShared::WebErrorCode_UniqueNickInUse:
			error = FESL_ERROR_ACCOUNT_EXISTS;
			break;
		case TaskShared::WebErrorCode_NoSuchUser:
			error = FESL_ERROR_ACCOUNT_NOT_FOUND;
			break;
		case TaskShared::WebErrorCode_NickInvalid:
			error = FESL_ERROR_ACCOUNT_NOT_FOUND;
			break;
		default:
			break;
		}
		SendError(cmd_type, error, TXN);
	}
	void Peer::handle_profile_search_callback_error(TaskShared::WebErrorDetails error_details, FESL_COMMAND_TYPE cmd_type, std::string TXN) {
		/*
		EProfileResponseType_Success,
		EProfileResponseType_GenericError,
		EProfileResponseType_BadNick,
		EProfileResponseType_Bad_OldNick,
		EProfileResponseType_UniqueNick_Invalid,
		EProfileResponseType_UniqueNick_InUse,
		*/
		FESL_ERROR error = FESL_ERROR_SYSTEM_ERROR;
		switch (error_details.response_code) {
			case TaskShared::WebErrorCode_UniqueNickInvalid:
			case TaskShared::WebErrorCode_NickInvalid:
				SendCustomError(cmd_type, TXN, "Account.ScreenName", "The account name was invalid. Please change and try again.");
				return;
			case TaskShared::WebErrorCode_NickInUse:
				//SendCustomError(cmd_type, TXN, "Account.ScreenName", "The account name is in use. Please choose another name.");
				error = FESL_ERROR_ACCOUNT_EXISTS;
				break;
			default:
				error = FESL_ERROR_SYSTEM_ERROR;
			break;
		}
		SendError(cmd_type, error, TXN);
		
	}
}