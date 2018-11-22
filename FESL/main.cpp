#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <OS/Net/NetServer.h>
#include <openssl/ssl.h>
#include <OS/Config/AppConfig.h>
#include "server/FESLServer.h"
#include "server/FESLDriver.h"
#include <OS/StringCrypter.h>
INetServer *g_gameserver = NULL;
bool g_running = true;

void shutdown();

void on_exit(void) {
    shutdown();
}

void sig_handler(int signo)
{
    shutdown();
}

std::string get_file_contents(std::string path) {
	std::string ret;
	FILE *fd = fopen(path.c_str(),"r");
	if(fd) {
		fseek(fd,0,SEEK_END);
		int len = ftell(fd);
		fseek(fd,0,SEEK_SET);

		char *str_data = malloc(len+1);
		fread(str_data, len, 1, fd);
		str_data[len] = 0;
		ret = str_data;
		free((void *)str_data);
	}
	fclose(fd);
	return ret;
}

SSLNetIOIFace::ESSL_Type getSSLVersion(std::string driver_name, AppConfig *app_config) {
	std::string ssl_version;

	app_config->GetVariableString(driver_name, "ssl-version", ssl_version);

	if (ssl_version.compare("SSLv2") == 0) {
		return SSLNetIOIFace::ESSL_SSLv2;
	}
	if (ssl_version.compare("SSLv23") == 0) {
		return SSLNetIOIFace::ESSL_SSLv23;
	}
	if (ssl_version.compare("SSLv3") == 0) {
		return SSLNetIOIFace::ESSL_SSLv3;
	}
	if (ssl_version.compare("TLS1.0") == 0) {
		return SSLNetIOIFace::ESSL_TLS10;
	}
	if (ssl_version.compare("TLS1.1") == 0) {
		return SSLNetIOIFace::ESSL_TLS11;
	}
	if (ssl_version.compare("TLS1.2") == 0) {
		return SSLNetIOIFace::ESSL_TLS12;
	}
	return SSLNetIOIFace::ESSL_None;
}

#include <curl/curl.h>
int main() {
	SSL_library_init();
	#ifndef _WIN32
		signal(SIGPIPE, SIG_IGN); // due to openssl SSL_write, we must block broken pipes here
		signal(SIGINT, sig_handler);
		signal(SIGTERM, sig_handler);
	#else
		WSADATA wsdata;
		WSAStartup(MAKEWORD(1, 0), &wsdata);
	#endif

	OS::Config *cfg = new OS::Config("openspy.xml");
	AppConfig *app_config = new AppConfig(cfg, "FESL");
	OS::Init("FESL", app_config);
	g_gameserver = new FESL::Server();

	std::vector<std::string> drivers = app_config->getDriverNames();
	std::vector<std::string>::iterator it = drivers.begin();
	while (it != drivers.end()) {
		std::string s = *it;

		std::vector<OS::Address> addresses = app_config->GetDriverAddresses(s);
		OS::Address address = addresses.front();

		SSLNetIOIFace::ESSL_Type ssl_version = getSSLVersion(s, app_config);

		std::string x509_path, rsa_path;

		std::string stringCrypterPKey;
		app_config->GetVariableString(s, "stringCrypterPKey", stringCrypterPKey);

		if (ssl_version != SSLNetIOIFace::ESSL_None) {
			app_config->GetVariableString(s, "x509", x509_path);
			app_config->GetVariableString(s, "x509_pkey", rsa_path);
		}

		FESL::PublicInfo server_info;

		std::string tos_path;
		app_config->GetVariableString(s, "tosFile", tos_path);
		app_config->GetVariableString(s, "domainPartition", server_info.domainPartition);
		app_config->GetVariableString(s, "subDomain", server_info.subDomain);

		app_config->GetVariableString(s, "messagingHostname", server_info.messagingHostname);
		
		int port;
		app_config->GetVariableInt(s, "messagingPort", port);
		server_info.messagingPort = (uint16_t)port;

		app_config->GetVariableString(s, "theaterHostname", server_info.theaterHostname);
		app_config->GetVariableInt(s, "theaterPort", port);
		server_info.theaterPort = (uint16_t)port;

		server_info.termsOfServiceData = get_file_contents(tos_path);

		FESL::Driver *driver = new FESL::Driver(g_gameserver, address, server_info, stringCrypterPKey, x509_path.c_str(), rsa_path.c_str(), ssl_version);
		OS::LogText(OS::ELogLevel_Info, "Adding FESL Driver: %s (ssl: %d)\n", address.ToString().c_str(), ssl_version != SSLNetIOIFace::ESSL_None);
		g_gameserver->addNetworkDriver(driver);
		it++;
	}

	g_gameserver->init();
	while(g_running) {
			g_gameserver->tick();
	}

    delete g_gameserver;

    OS::Shutdown();
    return 0;
}
void shutdown() {
    if(g_running) {
        g_running = false;
    }
}
