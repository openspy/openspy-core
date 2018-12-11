#include <tasks/tasks.h>
#include <server/SBServer.h>
#include <server/SBDriver.h>
namespace MM {
    bool Handle_ServerEventMsg(TaskThreadData *thread_data, std::string message) {
		std::vector<std::string> vec = OS::KeyStringToVector(message);
		if (vec.size() < 2) return;
		std::string msg_type = vec.at(0);
		std::string server_key = vec.at(1);


		MM::Server *server = GetServerByKey(thread_data, server_key, msg_type.compare("del") == 0);
		if(!server) return;

		MM::Server server_cpy = *server;
		delete server;

		if(msg_type.compare("del") == 0) {
			((SBServer *)thread_data->server)->OnDeleteServer(server_cpy);
		} else if(msg_type.compare("new") == 0) {
			((SBServer *)thread_data->server)->OnNewServer(server_cpy);
		} else if(msg_type.compare("update") == 0) {
			((SBServer *)thread_data->server)->OnUpdateServer(server_cpy);
		}
        return true;
    }
}