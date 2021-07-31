#include <server/QRServer.h>
#include <server/QRDriver.h>
#include <tasks/tasks.h>
#include <sstream>
namespace MM {
    bool Handle_QRMessage(TaskThreadData *thread_data, std::string message) {
		QR::Server *server = (QR::Server *)thread_data->server;

		json_t *root = json_loads(message.c_str(), 0, NULL);
		if(!root) return false;

		json_t *driver_address_json = json_object_get(root, "driver_address");
		json_t *to_address_json = json_object_get(root, "to_address");

		json_t *hostname_json = json_object_get(root, "hostname");

		bool should_process = true;

		if(hostname_json && json_is_string(hostname_json)) {
			const char *hostname = json_string_value(hostname_json);
			if(stricmp(OS::g_hostName, hostname) != 0) {
				should_process = false;
			}
		}

		int version = 0;
		json_t *version_obj = json_object_get(root, "version");
		if(json_is_integer(version_obj)) {
			version = json_integer_value(version_obj);
		}

		uint32_t instance_key = 0;
		json_t *instance_key_json = json_object_get(root, "instance_key");
		if(instance_key_json && json_is_integer(instance_key_json)) {
			instance_key = json_integer_value(instance_key_json);
		}

		int message_key = 0;
		json_t *message_key_json = json_object_get(root, "identifier");
		if(message_key_json && json_is_integer(message_key_json)) {
			message_key = json_integer_value(message_key_json);
		}

		OS::Buffer message_buffer;
		uint8_t *data_out;
		size_t data_len;

		json_t *message_json = json_object_get(root, "message");
		std::string base64_string;
		if(message_json && json_is_string(message_json)) {
			 base64_string = json_string_value(message_json);
		}

		OS::Base64StrToBin((const char *)base64_string.c_str(), &data_out, data_len);
		message_buffer.WriteBuffer(data_out, data_len);
		free(data_out);


		if(should_process && driver_address_json) {
			OS::Address driver_address;
			driver_address = OS::Address(json_string_value(driver_address_json));

			OS::Address to_address = OS::Address(json_string_value(to_address_json));
			

			QR::Driver *driver = server->findDriverByAddress(driver_address);

			if(driver) {
				json_t *type = json_object_get(root, "type");
				if(type) {
					const char *type_str = json_string_value(type);
					if(stricmp(type_str, "client_message") == 0) {
						if(version == 2) {
							driver->send_client_message(version, to_address, instance_key, message_key, message_buffer);
						}
					}
				}
			}
		}
		json_decref(root);
		return true;
    }
}