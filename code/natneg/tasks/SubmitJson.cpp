#include <algorithm>

#include <sstream>

#include "tasks.h"

#include <OS/tasks.h>

namespace NN {
    bool PerformSubmitJson(NNRequestData request) {
        TaskShared::sendAMQPMessage(nn_channel_exchange, "natneg.core", request.send_string.c_str());		
        return true;
    }
}