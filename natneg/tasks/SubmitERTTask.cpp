#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
#include "../server/NNPeer.h"
#include <algorithm>

#include <OS/TaskPool.h>
#include <OS/Redis.h>

#include <sstream>

#include "tasks.h"

#include <OS/MessageQueue/rabbitmq/rmqConnection.h>

namespace NN {
    bool PerformERTTest(NNRequestData request, TaskThreadData  *thread_data) {
		OS::Address address = request.peer->getAddress();
		std::ostringstream ss;
		ss << "\\msg\\natneg_erttest\\address\\" << address.ToString() << "\\type\\" << (request.type == ENNRequestType_PerformERTTest_IPUnsolicited);
		thread_data->mp_mqconnection->sendMessage(nn_channel_exchange, nn_channel_routingkey,ss.str());
		if(request.peer) {
			request.peer->DecRef();
		}
        return true;
    }
}