#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
#include <algorithm>

#include <OS/TaskPool.h>
#include <OS/Redis.h>

#include <sstream>

#include "tasks.h"

#include <OS/MessageQueue/rabbitmq/rmqConnection.h>

namespace NN {
    bool PerformSubmitJson(NNRequestData request, TaskThreadData  *thread_data) {
        MQ::MQMessageProperties props;
        props.exchange = nn_channel_exchange;
        props.routingKey = nn_channel_routingkey;
        props.headers["appName"] = OS::g_appName;
        props.headers["hostname"] = OS::g_hostName;
		thread_data->mp_mqconnection->sendMessage(props ,request.send_string.c_str());
        return true;
    }
}