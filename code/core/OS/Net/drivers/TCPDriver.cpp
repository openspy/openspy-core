#include "TCPDriver.h"
namespace OS {
    TCPDriver::TCPDriver(INetServer *server, const char *host, uint16_t port) : INetDriver(server) {
        mp_peers = new OS::LinkedListHead<INetPeer*>();

        struct sockaddr_in saddr;
        uv_ip4_addr(host, port, &saddr);

        gettimeofday(&m_server_start, NULL);


        uv_tcp_init(uv_default_loop(), &m_listener_socket);
        int r = uv_tcp_bind(&m_listener_socket, (const sockaddr *)&saddr, 0);
        if(r != 0) {
            OS::LogText(OS::ELogLevel_Error, "[%s:%d] Failed to bind TCP Address: %s", host, port, uv_strerror(r));
        }
        uv_handle_set_data((uv_handle_t*) &m_listener_socket, this);

        r = uv_listen((uv_stream_t *)&m_listener_socket, 128, TCPDriver::on_new_connection);
        if(r != 0) {
            OS::LogText(OS::ELogLevel_Error, "[%s:%d] Failed to start listener: %s for address", host, port, uv_strerror(r));
        }
    }
    TCPDriver::~TCPDriver() {
        DeleteClients();
        delete mp_peers;
    }
    void TCPDriver::on_new_connection(uv_stream_t *server, int status) {
        TCPDriver *driver = (TCPDriver*)uv_handle_get_data((uv_handle_t*)server);
        INetPeer *peer = driver->CreatePeer((uv_tcp_t *)server);
        //peer->SetAddress(sda->address);
        driver->mp_peers->AddToList(peer);

    }
    void TCPDriver::think() {
        TickConnections();
    }

    void TCPDriver::TickConnections() {
        OS::LinkedListIterator<INetPeer*, TCPDriver*> iterator(mp_peers);
        iterator.Iterate(LLIterator_TickOrDeleteClient, this);
    }
    bool TCPDriver::LLIterator_TickOrDeleteClient(INetPeer* peer, TCPDriver* driver) {
        if (peer->ShouldDelete()) {
            if (peer->GetRefCount() == 1) { //only 1 reference (the drivers reference)
                driver->mp_peers->RemoveFromList(peer);
                peer->DecRef();
                peer->CloseSocket(); //this will delete itself
            }
        }
        else {
            peer->think();
        }
        return true;
    }
    bool TCPDriver::LLIterator_DeleteAllClients(INetPeer* peer, TCPDriver* driver) {
        //no need to remove from linked list, since this method is *only* called on TCPDriver delete
        delete peer;
        return true;
    }
    void TCPDriver::DeleteClients() {
        OS::LinkedListIterator<INetPeer*, TCPDriver*> iterator(mp_peers);
        iterator.Iterate(LLIterator_DeleteAllClients, this);
    }
    
}