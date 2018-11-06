#include "common.h"

#include <OS/Redis.h>


#include "server/SBServer.h"
#include "server/SBDriver.h"
#include "server/SBPeer.h"
#include "server/V1Peer.h"

#include <OS/KVReader.h>

#define FAKE_PUBLIC_IP 0xAABBCCDD

#define LOOKUP_GAMENAME "thugpro"

//send cb can be called from different thread
bool Fake_SendCallback(INetIOSocket *socket, OS::Buffer buffer) {
	printf("Fake_SendCallback - %s\n", buffer.GetHead());
	buffer.resetReadCursor();
    int len = buffer.readRemaining();
    OS::KVReader kv_parser = OS::KVReader((const char *)buffer.GetHead());

	SBTestState *state = findTestStateByAddress(socket->address);
	if(state->peer == NULL) {
		MapTestConnections(state->driver); //since the server validation packet is sent prior to any client request, we must map it here, otherwise the peer could be null due to thread timing race condition
	}

	if(state) {
		if(!state->sent_validation_response) {
			std::string message_type = kv_parser.GetKeyByIdx(0);
			if(message_type.compare("basic") == 0) {
				std::string secure = kv_parser.GetValue("secure");
				state->server_challenge = secure;
				Send_ConnectionInitPacket(state);
			}	
		} else {
			std::vector<std::string> temp_fields;
			std::vector<std::string>::iterator field_it;
			int field_count;
			switch(state->parse_state) {
				case EParseState_ReadFields:
					field_count = kv_parser.GetValueInt("fieldcount");
					temp_fields = OS::KeyStringToVector((const char *)buffer.GetHead());
					field_it = temp_fields.begin(); field_it += 2;
					while(field_it != temp_fields.end()) {
						state->fields.push_back((*field_it));
						if(state->fields.size() == field_count) break;
						field_it++;
					}
					state->parse_state = EParseState_ReadList;
				break;
				case EParseState_ReadList:
				printf("READ LIST\n");
				//temp_fields = OS::KeyStringToVector((const char *)buffer.GetHead());
				std::vector< std::map<std::string, std::string> > entries = OS::ValueStringToMapArray(state->fields, (const char *)buffer.GetHead());
				
				
				if(entries[0]["hostname"].compare("Beenox") == 0) {
					exit(0);
				} else {
					exit(1);
				}
				break;
			}
		}

	}

	return false;

}


int main() {
	OS::Init("serverbrowsing", "openspy.cfg");

	test_gameserver = new SBServer();
	test_gameserver->init();

	test_netio_iface = new Test::TestNetIOInterface<>();
	test_netio_iface->fake_SetSendCallback(Fake_SendCallback);

	test_gameserver->setNetIOInterface(test_netio_iface);
	
    SB::Driver *driver = new SB::Driver(test_gameserver, "127.0.0.1", 28900, 1);

	test_gameserver->addNetworkDriver(driver);

	OS::Sleep(1000);

    OS::Address address;
    address.ip = FAKE_PUBLIC_IP;
    address.port = rand();
    

    Make_FakeConnection(driver, address);

	driver->think(true); //trigger TCP accept
	MapTestConnections(driver);

	OS::Sleep(5000); //must be higher than thread timer

	std::map<OS::Address, SBTestState *>::iterator it = m_test_connections.begin();


	while (it != m_test_connections.end()) {
		SBTestState *state = (*it).second;
		Send_FakeServerGroupsReq(state, LOOKUP_GAMENAME);
		it++;
	}

	OS::Sleep(75000);

    /*
	
	

	it = m_test_connections.begin();
	while (it != m_test_connections.end()) {
		std::pair<OS::Address, SBTestState *> entry = *it;

		Test::TestNetIOInterface<> *net_iface = (Test::TestNetIOInterface<> *)entry.second->driver->getServer()->getNetIOInterface();

		net_iface->fake_TCPDisconnect(entry.second->socket);
		entry.second->peer->DecRef();
		entry.second->peer->think(true);
		it++;
	}
    */
    return -1;
}