#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
typedef int socklen_t;
#define sleep ::Sleep
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h> 
#include <sys/time.h>
#endif
#include <stdarg.h>
#include "Redis.h"

#include <OS/OpenSpy.h>
#define REDIS_BUFFSZ 1000000
#define RECONNECT_SLEEP_TIME 2000
namespace Redis {
	uint32_t resolv(const char *host) {
		struct  hostent *hp;
		uint32_t    host_ip;

		host_ip = inet_addr(host);
		if (host_ip == INADDR_NONE) {
			hp = gethostbyname(host);
			if (!hp) {
				return (uint32_t)-1;
			}
			host_ip = *(uint32_t *)hp->h_addr;
		}
		return(host_ip);
	}

	void get_server_address_port(const char *input, char *address, uint16_t &port) {
		const char *seperator = strrchr(input, ':');
		int len = strlen(input);
		if (seperator) {
			port = atoi(seperator + 1);
			len = seperator - input;
		}
		strncpy(address, input, len);
		address[len] = 0;
	}

	Connection *Connect(const char *constr, struct timeval tv) {

		Connection *ret = (Connection *)malloc(sizeof(Connection));
		memset(ret, 0, sizeof(Connection));
		char address[64];
		uint16_t port;
		get_server_address_port(constr, address, port);

		ret->connect_address = std::string(constr);
		ret->command_recursion_depth = 0;
		ret->reconnect_recursion_depth = 0;

		ret->read_buff_alloc_sz = REDIS_BUFFSZ;
		ret->read_buff = (char *)malloc(REDIS_BUFFSZ);

		ret->runLoop = false;

		performAddressConnect(ret, address, port);

		return ret;
	}
	void Reconnect(Connection *connection) {
		char address[64];
		uint16_t port;
		get_server_address_port(connection->connect_address.c_str(), address, port);

		close(connection->sd);
		OS::Sleep(RECONNECT_SLEEP_TIME);
		performAddressConnect(connection, address, port);
		connection->reconnect_recursion_depth = 0;
	}
	void performAddressConnect(Connection *connection, const char *address, uint16_t port) {
		connection->sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		//setsockopt(ret->sd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(struct timeval));
		uint32_t ip = resolv(address);

		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = ip;
		int r = connect(connection->sd, (sockaddr *)&addr, sizeof(addr));
		if (r < 0) {
			OS::LogText(OS::ELogLevel_Critical, "redis connect error (%s:%d) (IP: %lu) ret: %d", address, port, ip, r);
			//error
			if(++connection->reconnect_recursion_depth > REDIS_MAX_RECONNECT_RECURSION_DEPTH) {
				return;
			}
			close(connection->sd);
			performAddressConnect(connection, address, port);
		}
	}
	std::string read_line(std::string str) {
		std::string r;
		for (int i = 0; i < str.length(); i++) {
			if (str[i] == '\r' && i + 1 < str.length()) {
				if (str[i + 1] == '\n') {
					return r;
				}
			}
			r += str[i];
		}

		return r;
	}
	Redis::ArrayValue read_array(std::string str, int &diff, Redis::Response *resp, Redis::ArrayValue *arr_val) {
		Redis::ArrayValue arr;
		std::string len_line = Redis::read_line(str).substr(1);
		int num_elements = atoi(len_line.c_str());
		diff += len_line.length() + ENDLINE_STR_COUNT + 1; // +1 for the operator
		if (diff > str.length()) {
			diff = str.length();
		}
		str = str.substr(diff);
		for (int i = 0; i < num_elements; i++) {
			int tdiff = 0;
			parse_response(str, tdiff, resp, &arr);
			str = str.substr(tdiff);
			diff += tdiff;
		}
		return arr;
	}
	Redis::Value read_scalar(std::string str, int &diff, Redis::Response *resp, Redis::ArrayValue *arr_val) {
		Redis::Value v;
		std::string info_line = Redis::read_line(str);
		int str_len = 0;
		int line_len = ENDLINE_STR_COUNT + info_line.length();
		//printf("Info Line: %s\n", info_line.c_str());
		diff += line_len;

		std::string info = str.substr(line_len);
		std::string data_str;
		switch (info_line[0]) {
		case '$': //bulk str
			str_len = atoi(info_line.substr(1).c_str());
			if (str_len == -1) {
				v.type = Redis::REDIS_RESPONSE_TYPE_NULL;
			}
			else {
				v.type = Redis::REDIS_RESPONSE_TYPE_STRING;
				data_str = info.substr(0, str_len);
				diff += str_len + ENDLINE_STR_COUNT;
				v.value._str = data_str;
			}
			break;
		case '+': //simple str
			v.type = Redis::REDIS_RESPONSE_TYPE_STRING;
			data_str = info_line.substr(1);
			//diff += info_line.length() + ENDLINE_STR_COUNT;
			v.value._str = data_str;
			break;
		case ':': //int
			v.type = Redis::REDIS_RESPONSE_TYPE_INTEGER;
			info = info_line.substr(1);
			v.value._int = atoi(info.c_str());
			break;
		case '-': //error
			v.type = Redis::REDIS_RESPONSE_TYPE_ERROR;
			data_str = info_line.substr(1);
			//diff += info_line.length() + ENDLINE_STR_COUNT;
			v.value._str = data_str;
			break;
		default:
			break;

		}

		return v;
	}
	void parse_response(std::string resp_str, int &diff, Redis::Response *resp, Redis::ArrayValue *arr_val) {
		std::string str = resp_str;
		Redis::ArrayValue av;
		Redis::Value v;
		int real_diff = diff;
		diff = 0;
		do {
			if (str.length() == 0) break;
			switch (str[0]) {
			case '*':
				av = read_array(str, diff, resp, arr_val);
				v.arr_value = av;
				v.type = Redis::REDIS_RESPONSE_TYPE_ARRAY;
				break;
			default:
				v = read_scalar(str, diff, resp, arr_val);
				break;
			}
			if (arr_val) {
				arr_val->values.push_back(std::pair<Redis::REDIS_RESPONSE_TYPE, struct Redis::_Value>(v.type, v));
			}
			else {
				resp->values.push_back(v);
			}
			if (diff > str.length()) {
				break;
			}
			str = str.substr(diff);
			real_diff += diff;
			diff = 0;

		} while (str.length());
		diff = real_diff;

	}
	int Recv(Connection *conn) {
		enum EReadState {
			READ_STATE_READ_OPERATOR,
			READ_STATE_READ_TO_NEWLINE,
		};
		struct {
			bool has_read_operator;
			int num_read_lines;

			bool recv_loop;

			EReadState state;
			char last_operator;

			int read_array_count_start_idx;
			int read_string_length;
			int read_string_start;
		} sReadLineData;

		sReadLineData.has_read_operator = false;
		sReadLineData.num_read_lines = 1;
		sReadLineData.state = READ_STATE_READ_OPERATOR;
		sReadLineData.recv_loop = true;
		int total_len = 0, len;
		while(sReadLineData.recv_loop) {
			len = recv(conn->sd, &conn->read_buff[total_len], conn->read_buff_alloc_sz-total_len, 0); //TODO: check if exeeds max len.. currently set to 1 MB so shouldn't...
			if (len <= 0) { return len; }
			while (len--) {
				switch (sReadLineData.state) {
				case READ_STATE_READ_OPERATOR:
					sReadLineData.read_string_start = 0;
					sReadLineData.state = READ_STATE_READ_TO_NEWLINE;
					sReadLineData.last_operator = conn->read_buff[total_len];
					switch (conn->read_buff[total_len]) {
					case '$':
						sReadLineData.num_read_lines++; //read string content
														//intentional fall-through
					case '*':
						sReadLineData.read_array_count_start_idx = total_len;
					}
					break;
				case READ_STATE_READ_TO_NEWLINE:
					if (conn->read_buff[total_len] == '\n') {
						if (sReadLineData.read_string_start == 0 || total_len - sReadLineData.read_string_start >= sReadLineData.read_string_length) {
							sReadLineData.state = READ_STATE_READ_OPERATOR;
							switch (sReadLineData.last_operator) {
							case '$':
								if (atoi(&conn->read_buff[sReadLineData.read_array_count_start_idx + 1]) == -1) {
									sReadLineData.num_read_lines--;
								}
								else {
									sReadLineData.read_string_start = total_len;
									sReadLineData.read_string_length = atoi(&conn->read_buff[sReadLineData.read_array_count_start_idx + 1]);
									sReadLineData.state = READ_STATE_READ_TO_NEWLINE;
									sReadLineData.last_operator = 0;
								}
								break;
							case '*':
								sReadLineData.num_read_lines += atoi(&conn->read_buff[sReadLineData.read_array_count_start_idx + 1]);
								break;
							}
							sReadLineData.num_read_lines--;
						}
					}
				}
				total_len++;
			}
			sReadLineData.recv_loop = sReadLineData.num_read_lines > 0;
		}
		conn->read_buff[total_len] = 0;
		return total_len;
	}
	Response Command(Connection *conn, time_t sleepMS, const char *fmt, ...) {
		Response resp;
		va_list args;
		va_start(args, fmt);

		vsprintf(conn->read_buff, fmt, args);

		std::string cmd = conn->read_buff + std::string("\r\n");
		send(conn->sd, cmd.c_str(), cmd.length(), 0);
		va_end(args);

		if (sleepMS != 0)
			OS::Sleep(sleepMS);
		int len = Recv(conn);
		if (len <= 0) {
			OS::LogText(OS::ELogLevel_Critical, "redis recv error: %d", len);
			Reconnect(conn);
			if (conn->command_recursion_depth < REDIS_MAX_RECONNECT_RECURSION_DEPTH) {
				conn->command_recursion_depth++;
				resp = Command(conn, sleepMS, "%s", cmd.c_str());
				conn->command_recursion_depth = 0;
				return resp;
			}
			else {
				conn->command_recursion_depth = 0;
			}
			return resp;
		}
		else {
			conn->command_recursion_depth = 0;
		}

		int diff = 0;
		parse_response(conn->read_buff, diff, &resp, NULL);
		return resp;
	}
	void LoopingCommand(Connection *conn, time_t sleepMS, void(*mpFunc)(Connection *, Response, void *), void *extra, const char *fmt, ...) {
		//TODO: handle sleep/big msgs
		int diff = 0;
		Response resp;
		va_list args;
		va_start(args, fmt);

		vsprintf(conn->read_buff, fmt, args);

		std::string cmd = conn->read_buff + std::string("\r\n");
		send(conn->sd, cmd.c_str(), cmd.length(), 0);
		va_end(args);

		if (sleepMS != 0) {
			OS::Sleep(sleepMS);
		}

		conn->runLoop = true;
		while (conn->runLoop) {
			if (Recv(conn) <= 0) {
				OS::Sleep(5000); //Sleep even longer due to async... more likely to be in a CPU consuming loop
				Reconnect(conn);
				send(conn->sd, cmd.c_str(), cmd.length(), 0);
				continue;
			}
			parse_response(conn->read_buff, diff, &resp, NULL);
			mpFunc(conn, resp, extra);
			diff = 0;
			resp.values.clear();
		}
	}
	void Disconnect(Connection *connection) {

		free(connection->read_buff);
		if(connection->sd != 0)
			close(connection->sd);

		free((void *)connection);
	}
	void CancelLooping(Connection *connection) {
		connection->runLoop = false;
		shutdown(connection->sd, 2);
		close(connection->sd);
		connection->sd = 0;
	}
	bool CheckError(Response r) {
		return r.values.size() == 0 || r.values.front().type == Redis::REDIS_RESPONSE_TYPE_ERROR;
	}
}