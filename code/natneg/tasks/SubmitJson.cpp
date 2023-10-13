#include <OS/Config/AppConfig.h>
#include <OS/Task/TaskScheduler.h>
#include <algorithm>

#include <OS/TaskPool.h>
#include <sstream>

#include "tasks.h"

#include <OS/MessageQueue/rabbitmq/rmqConnection.h>

namespace NN {
    bool PerformSubmitJson(NNRequestData request, TaskThreadData  *thread_data) {
        // MQ::MQMessageProperties props;
        // props.exchange = nn_channel_exchange;
        // props.routingKey = nn_channel_routingkey;
        // props.headers["appName"] = OS::g_appName;
        // props.headers["hostname"] = OS::g_hostName;

		amqp_connection_state_t connection = getThreadLocalAmqpConnection();

		amqp_basic_properties_t props;
		props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
		props.content_type = amqp_cstring_bytes("text/plain");
		props.delivery_mode = 1;

        amqp_table_entry_t entries[2];
        props.headers.num_entries = 2;
        props.headers.entries = (amqp_table_entry_t*)&entries;

        entries[0].key = amqp_cstring_bytes("appName");
        entries[0].value.kind = AMQP_FIELD_KIND_UTF8;
        entries[0].value.value.bytes = amqp_cstring_bytes(OS::g_appName);

        entries[1].key = amqp_cstring_bytes("hostname");
        entries[1].value.kind = AMQP_FIELD_KIND_UTF8;
        entries[1].value.value.bytes = amqp_cstring_bytes(OS::g_hostName);

		amqp_basic_publish(connection, 1, amqp_cstring_bytes(nn_channel_exchange),
                                    amqp_cstring_bytes(nn_channel_routingkey), 0, 0,
                                    &props, amqp_cstring_bytes(request.send_string.c_str()));
		
        return true;
    }
}