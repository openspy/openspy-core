#include "common.h"

#include <OS/Redis.h>

#include "server/QRServer.h"
#include "server/QRDriver.h"

#define NUM_PEERS 5

enum ETestIdentifiers {
    TestIdentifier_AddServer = 0,
    TestIdentifier_AddServer_InvalidPlayers
};

std::vector<QR2TestState *> test_states;


struct _TestPassState {
    bool passed_server_register_test;
    bool passed_server_invalid_player_length_test;
} TestPassState;

void Test_Handle_QR2Registered(QR2TestState *state, OS::Buffer buffer) {
	uint16_t magic;
	uint32_t instance_key;

	buffer.resetReadCursor();
	magic = buffer.ReadShort();
	instance_key = buffer.ReadInt();

    printf("registered\n");
    if(state->test_type == ETestType_ServerRegister_Fail && state->identifier == TestIdentifier_AddServer_InvalidPlayers) {
        TestPassState.passed_server_invalid_player_length_test = false;
    } else if(state->test_type == ETestType_ServerRegister) {
        TestPassState.passed_server_register_test = true;
    }
}

void Send_FakeServerMsgReq_InvalidPlayerLength(QR2TestState *test_state) {
	OS::Buffer buff;
	buff.WriteByte(PACKET_HEARTBEAT); //request type
    buff.WriteInt(test_state->instance_key);
	
    buff.WriteNTS("hostname");
    buff.WriteNTS("test server");

    buff.WriteNTS("gamename");
    buff.WriteNTS(TEST_GAMENAME);

    buff.WriteNTS("statechanged");
    buff.WriteNTS("1");

    buff.WriteByte(0); //main KV terminator

    buff.WriteShort(1); //num players

    buff.WriteNTS("player_");
    buff.WriteByte(0);
    
    buff.WriteNTS("test player");

	buff.resetReadCursor();

    test_state->test_type = ETestType_ServerRegister_Fail;
    test_state->identifier = TestIdentifier_AddServer_InvalidPlayers;


    INetIODatagram dgram;
    dgram.address = test_state->address;
    dgram.comm_len = buff.bytesWritten();
    dgram.buffer = buff;

    test_netio_iface->fake_PushIncomingDatagram(test_state->driver->getListenerSocket(), dgram);
	test_state->driver->think(true);
}

void Send_FakeServerMsgReq_AddServer(QR2TestState *test_state) {
	OS::Buffer buff;
	buff.WriteByte(PACKET_HEARTBEAT); //request type
    buff.WriteInt(test_state->instance_key);
	
    buff.WriteNTS("hostname");
    buff.WriteNTS("test server");

    buff.WriteNTS("gamename");
    buff.WriteNTS(TEST_GAMENAME);

    buff.WriteNTS("statechanged");
    buff.WriteNTS("1");

    buff.WriteByte(0); //main KV terminator

    buff.WriteShort(htons(1)); //num players

    buff.WriteNTS("player_");
    buff.WriteByte(0);

    buff.WriteNTS("test player");

    buff.WriteByte(0);
    buff.WriteShort(0);

	buff.resetReadCursor();


    INetIODatagram dgram;
    dgram.address = test_state->address;
    dgram.comm_len = buff.bytesWritten();
    dgram.buffer = buff;


    test_state->test_type = ETestType_ServerRegister;
    test_state->identifier = TestIdentifier_AddServer;

    test_netio_iface->fake_PushIncomingDatagram(test_state->driver->getListenerSocket(), dgram);
	test_state->driver->think(true);
}

bool Fake_SendCallback(INetIOSocket *socket, OS::Buffer buffer) {

    QR2TestState *state = findTestStateByAddress(socket->address);

    switch(state->state) {
        case ETestState_WaitChallenge:
            return state->challenge_handler(state, buffer);
        break;
        case ETestState_WaitRegistered:
            return state->registered_handler(state, buffer);
        break;
    }
	return false;
}


int main() {
    memset(&TestPassState,0, sizeof(TestPassState));

    TestPassState.passed_server_invalid_player_length_test = true; //default to true, unless register event sent
	OS::Init("qr", "openspy.cfg");

	test_gameserver = new QR::Server();
	test_gameserver->init();

	test_netio_iface = new Test::TestNetIOInterface<>();
	test_netio_iface->fake_SetSendCallback(Fake_SendCallback);

	test_gameserver->setNetIOInterface(test_netio_iface);
	
    QR::Driver *driver = new QR::Driver(test_gameserver, "127.0.0.1", 27900);

	test_gameserver->addNetworkDriver(driver);

	OS::Sleep(1000);

    for(int i=0;i<NUM_PEERS;i++) {
        OS::Address address;
        address.ip = rand();
        address.port = rand();
        QR2TestState *state = Make_FakeConnection(driver, address);
        state->challenge_handler = Handle_QR2Challenge;
        state->registered_handler = Test_Handle_QR2Registered;
        test_states.push_back(state);
    }


	driver->think(true); //trigger TCP accept
	MapTestConnections(driver);

    Send_FakeServerMsgReq_InvalidPlayerLength(test_states[0]);

    Send_FakeServerMsgReq_AddServer(test_states[1]);

    OS::Sleep(10000);
    driver->think(true); //trigger TCP accept

    if(!TestPassState.passed_server_register_test) {
        return -1;
    }
    if(!TestPassState.passed_server_invalid_player_length_test) {
        return -2;
    }

    return 0;
}