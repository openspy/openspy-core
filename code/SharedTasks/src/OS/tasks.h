#ifndef _TASKS_SHARED_H
#define _TASKS_SHARED_H
#include <string>
#include <hiredis/hiredis.h>
namespace TaskShared {
	class TaskThreadData {
		public:
			redisContext *mp_redis_connection;
	};

	struct curl_data {
		std::string buffer;
	};

    size_t curl_callback(void *contents, size_t size, size_t nmemb, void *userp);
}
#endif //_TASKS_SHARED_H