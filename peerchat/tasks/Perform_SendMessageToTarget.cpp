#include "tasks.h"
#include <sstream>
#include <server/Server.h>

#define USER_EXPIRE_TIME 300
namespace Peerchat {
    bool Handle_Message(TaskThreadData *thread_data, std::string message) {
        uint8_t *data_out;
		size_t data_len;

        Peerchat::Server *server = (Peerchat::Server *)thread_data->server;
        OS::KVReader reader = OS::KVReader(message);

        
        OS::Base64StrToBin((const char *)reader.GetValue("message").c_str(), &data_out, data_len);
        server->OnUserMessage(reader.GetValue("type"), reader.GetValue("from"), reader.GetValue("to"), (const char *)data_out);
        free(data_out);
    }
    
    bool Perform_SendMessageToTarget(PeerchatBackendRequest request, TaskThreadData *thread_data) {
        OS::Sleep(1000);
        printf("** SEND MSG: %s\n", request.message.c_str());
		const char *base64 = OS::BinToBase64Str((uint8_t *)request.message.c_str(), request.message.length());
		std::string b64_string = base64;
		free((void *)base64);

		std::ostringstream message;
		message << "\\type\\" << request.message_type.c_str() << "\\to\\" << request.message_target << "\\message\\" << b64_string << "\\from\\" << request.summary.ToString();


        thread_data->mp_mqconnection->sendMessage(peerchat_channel_exchange, peerchat_client_message_routingkey, message.str().c_str());
        return true;
    }
}