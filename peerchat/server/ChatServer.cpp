#include "ChatPeer.h"
#include "ChatServer.h"
#include "ChatDriver.h"
ChatServer::ChatServer() : INetServer() {
	m_name = "s-os-1";
}
ChatServer::~ChatServer() {
}
void ChatServer::init() {
}
void ChatServer::tick() {
	NetworkTick();
}
void ChatServer::shutdown() {

}
