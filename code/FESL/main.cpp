#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>

#include <OS/Net/NetPeer.h>
#include "server/FESLServer.h"
#include "server/FESLDriver.h"
#include <SSL/StringCrypter.h>
#include <OS/tasks.h>

#include <SSL/SSLTCPDriver.h>

INetServer *g_gameserver = NULL;


void tick_handler(uv_timer_t* handle) {
	g_gameserver->tick();
}

std::string get_file_contents(std::string path) {
	std::string ret;
	FILE *fd = fopen(path.c_str(),"r");
	if(fd) {
		fseek(fd,0,SEEK_END);
		int len = ftell(fd);
		fseek(fd,0,SEEK_SET);

		char *str_data = (char *)malloc(len+1);
		fread(str_data, len, 1, fd);
		str_data[len] = 0;
		ret = str_data;
		free((void *)str_data);
	}
	fclose(fd);
	return ret;
}

std::string GetStringCryptPrivateKey() {
	std::string stringCrypterPKey;

	char env_buffer[256];
	size_t temp_env_sz = sizeof(env_buffer);

	if(uv_os_getenv("OPENSPY_FESL_STRCRYPT_PKEY", (char *)&env_buffer, &temp_env_sz) == 0) {
			stringCrypterPKey = std::string(env_buffer, temp_env_sz);
	}
	return stringCrypterPKey;
}
FESL::PublicInfo GetPublicInfo() {
	char env_buffer[256];
	size_t temp_env_sz = sizeof(env_buffer);

	FESL::PublicInfo result;

	if(uv_os_getenv("OPENSPY_FESL_TOS_PATH", (char *)&env_buffer, &temp_env_sz) == 0) {
			result.termsOfServiceData = get_file_contents(std::string(env_buffer, temp_env_sz));
	}

	if(uv_os_getenv("OPENSPY_FESL_DOMAIN_PARTITION", (char *)&env_buffer, &temp_env_sz) == 0) {
			result.domainPartition = std::string(env_buffer, temp_env_sz);
	}

	if(uv_os_getenv("OPENSPY_FESL_SUBDOMAIN", (char *)&env_buffer, &temp_env_sz) == 0) {
			result.subDomain = std::string(env_buffer, temp_env_sz);
	}

	if(uv_os_getenv("OPENSPY_FESL_MESSAGINGHOSTNAME", (char *)&env_buffer, &temp_env_sz) == 0) {
			result.messagingHostname = std::string(env_buffer, temp_env_sz);
	}

	if(uv_os_getenv("OPENSPY_FESL_MESSAGINGPORT", (char *)&env_buffer, &temp_env_sz) == 0) {
			std::string input = std::string(env_buffer, temp_env_sz);
			result.messagingPort = atoi(input.c_str());
	}

	if(uv_os_getenv("OPENSPY_FESL_THEATREHOSTNAME", (char *)&env_buffer, &temp_env_sz) == 0) {
			result.theaterHostname = std::string(env_buffer, temp_env_sz);
	}

	if(uv_os_getenv("OPENSPY_FESL_THEATREPORT", (char *)&env_buffer, &temp_env_sz) == 0) {
			std::string input = std::string(env_buffer, temp_env_sz);
			result.theaterPort = atoi(input.c_str());
	}

	if(uv_os_getenv("OPENSPY_FESL_GAMEID", (char *)&env_buffer, &temp_env_sz) == 0) {
			std::string input = std::string(env_buffer, temp_env_sz);
			result.gameid = atoi(input.c_str());
	}
	return result;
}

int main() {
	uv_loop_t *loop = uv_default_loop();
	uv_timer_t tick_timer;

	uv_timer_init(uv_default_loop(), &tick_timer);

	OS::Init("FESL");
	g_gameserver = new FESL::Server();


	char address_buff[256];
	char port_buff[16];
	size_t temp_env_sz = sizeof(address_buff);

	if(uv_os_getenv("OPENSPY_FESL_BIND_ADDR", (char *)&address_buff, &temp_env_sz) == 0) {
		temp_env_sz = sizeof(port_buff);
		uint16_t port = 18000;
		if(uv_os_getenv("OPENSPY_FESL_BIND_PORT", (char *)&port_buff, &temp_env_sz) == 0) {
			port = atoi(port_buff);
		} else {
			OS::LogText(OS::ELogLevel_Warning, "Failed to get FESL bind port environment variable");
		}

		FESL::PublicInfo public_info = GetPublicInfo();
		std::string str_crypter_rsa_key = GetStringCryptPrivateKey();

		void *ssl_ctx = OS::GetSSLContext();

		FESL::Driver *driver = new FESL::Driver(g_gameserver, address_buff, port, public_info, str_crypter_rsa_key, ssl_ctx);

		OS::LogText(OS::ELogLevel_Info, "Adding FESL Driver: %s:%d\n", address_buff, port);
		g_gameserver->addNetworkDriver(driver);
	} else {
		OS::LogText(OS::ELogLevel_Warning, "Failed to get FESL bind address environment variable");
	}

	FESL::InitTasks();
	((FESL::Server *)g_gameserver)->init();
	uv_timer_start(&tick_timer, tick_handler, 0, 250);

	uv_run(loop, UV_RUN_DEFAULT);

	uv_loop_close(loop);

	delete g_gameserver;
	OS::Shutdown();
	return 0;
}