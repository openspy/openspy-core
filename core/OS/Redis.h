#ifndef _OS_REDIS_H
#define _OS_REDIS_H

#include <vector>
#include <string>
#include <time.h>

#define REDIS_MAX_RECONNECT_RECURSION_DEPTH 5

namespace Redis {

	enum REDIS_RESPONSE_TYPE {
		REDIS_RESPONSE_TYPE_ERROR,
		REDIS_RESPONSE_TYPE_ARRAY,
		REDIS_RESPONSE_TYPE_INTEGER,
		REDIS_RESPONSE_TYPE_STRING,
		REDIS_RESPONSE_TYPE_NULL,
	};

	typedef struct _ScalarValue {
		std::string _str;
		int         _int;
	} ScalarValue;

	typedef struct _Value;
	typedef struct _ArrayValue {
		std::vector<std::pair<REDIS_RESPONSE_TYPE, struct _Value> > values;
	} ArrayValue;


	typedef struct _Value {
		struct _ScalarValue value;
		struct _ArrayValue arr_value;
		REDIS_RESPONSE_TYPE type;
	} Value;

	typedef struct {
		int sd;
		char *read_buff;
		int read_buff_alloc_sz;
		int command_recursion_depth;
		int reconnect_recursion_depth;
		bool runLoop;
		std::string connect_address;
	} Connection;

	typedef struct {
		std::vector<struct _Value> values; //vector because can be multiple command responses
	} Response;

#define ENDLINE_STR_COUNT 2

	//TODO: PING/RECONNECT, 

	Connection *Connect(const char *hostname, struct timeval tv);
	Response Command(Connection *conn, time_t sleepMS, const char *fmt, ...);
	void LoopingCommand(Connection *conn, time_t sleepMS, void(*mpFunc)(Connection *, Response, void *), void *extra, const char *fmt, ...); //for SUBSCRIBE/DEBUGGER, etc
	void Disconnect(Connection *connection);
	void parse_response(std::string resp_str, int &diff, Redis::Response *resp, Redis::ArrayValue *arr_val);
	bool CheckError(Response r);
	void Reconnect(Connection *connection);
	void performAddressConnect(Connection *connection, const char *address, uint16_t port);
}
#endif //_OS_REDIS_H
