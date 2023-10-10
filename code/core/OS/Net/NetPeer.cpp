#include <OS/OpenSpy.h>
#include "NetDriver.h"
#include "NetPeer.h"

void INetPeer::stream_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    INetPeer *peer = (INetPeer*)uv_handle_get_data((const uv_handle_t*)stream);
    printf("net peer read: %d\n", nread);
    if (nread < 0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        }
        peer->Delete();
    } else if (nread > 0) {        
        peer->on_stream_read(stream, nread, buf);
    }

    if (buf->base) {
        free(buf->base);
    }
}