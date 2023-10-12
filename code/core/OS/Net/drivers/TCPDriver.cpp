#include "TCPDriver.h"
namespace OS {
    TCPDriver::TCPDriver(INetServer *server, const char *host, uint16_t port, bool proxyHeaders, const char *x509_path, const char *rsa_priv_path, ESSL_Type ssl_version) : INetDriver(server) {
        struct sockaddr_in saddr;
        uv_ip4_addr(host, port, &saddr);

        gettimeofday(&m_server_start, NULL);

        m_proxy_headers = proxyHeaders;

        uv_tcp_init(uv_default_loop(), &m_listener_socket);
        uv_tcp_bind(&m_listener_socket, (const sockaddr *)&saddr, 0);
        uv_handle_set_data((uv_handle_t*) &m_listener_socket, this);

        uv_listen((uv_stream_t *)&m_listener_socket, 128, TCPDriver::on_new_connection);
    }
    TCPDriver::~TCPDriver() {
        DeleteClients();
    }
    void TCPDriver::on_new_connection(uv_stream_t *server, int status) {
        TCPDriver *driver = (TCPDriver*)uv_handle_get_data((uv_handle_t*)server);
        INetPeer *peer = driver->CreatePeer((uv_tcp_t *)server);
        //peer->SetAddress(sda->address);
        driver->m_server->RegisterSocket(peer, driver->m_proxy_headers);
        driver->mp_peers->AddToList(peer);

    }
    void TCPDriver::think(bool listener_waiting) {
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
                driver->m_server->UnregisterSocket(peer);
                peer->DecRef();
                peer->CloseSocket(); //this will delete itself
            }
        }
        else {
            peer->think(false);
        }
        return true;
    }
    void TCPDriver::OnPeerMessage(INetPeer *peer) {
        // OS::Address source_address, proxy_server_address;
        // if(!peer->GetSocket()) {
        //     return;
        // }
        // bool success = getNetIOInterface()->ReadProxyAddress(peer->GetSocket(), source_address, proxy_server_address);

        // if(success) {
        //     m_server->UnregisterSocket(peer);
        //     m_server->RegisterSocket(peer);
        //     peer->SetAddress(source_address);
        //     peer->OnConnectionReady();
        // }
    }
    bool TCPDriver::LLIterator_DeleteAllClients(INetPeer* peer, TCPDriver* driver) {
        //no need to remove from linked list, since this method is *only* called on TCPDriver delete
        driver->m_server->UnregisterSocket(peer);
        delete peer;
        return true;
    }
    void TCPDriver::DeleteClients() {
        OS::LinkedListIterator<INetPeer*, TCPDriver*> iterator(mp_peers);
        iterator.Iterate(LLIterator_DeleteAllClients, this);
    }
    
}