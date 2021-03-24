/*
	This buffer class manages itself to prevent memory leaks, and auto-reallocs when needed, but is not thread safe.
*/
#include "Buffer.h"
#define REALLOC_ADD_SIZE 512
#define BUFFER_SAFE_SIZE 128 //if this amount of bytes isn't available, realloc
namespace OS {
		BufferCtx::BufferCtx(size_t alloc_size) : OS::Ref() {
			_head = malloc(alloc_size);
			pointer_owner = true;
			this->alloc_size = alloc_size;
		}
		BufferCtx::BufferCtx(void *addr, size_t len) : OS::Ref() {
			pointer_owner = false; 
			_head = addr;
			alloc_size = len;
		}
		BufferCtx::~BufferCtx() {
			if (pointer_owner) {
				free((void *)_head);
			}
		}
		Buffer::Buffer(void *addr, size_t len) {
			mp_ctx = new BufferCtx(addr, len);
			resetCursors();
		}
		Buffer::Buffer(size_t alloc_size) {
			mp_ctx = new BufferCtx(alloc_size);
			resetCursors();
		}	
		Buffer::Buffer(const Buffer &cpy) {			
			cpy.mp_ctx->IncRef();
			mp_ctx = cpy.mp_ctx;
			_read_cursor = cpy._read_cursor;
			_write_cursor = cpy._write_cursor;
		}
		Buffer::Buffer() {
			mp_ctx = new BufferCtx(1492);
			resetCursors();
		}
		Buffer::~Buffer() {
			mp_ctx->DecRef();
			int ref = mp_ctx->GetRefCount();
			if (ref == 0) {
				delete mp_ctx;
			}
		}	
		uint8_t Buffer::ReadByte() {
			if (readRemaining() < sizeof(uint8_t)) return 0;
			uint8_t val = *(uint8_t *)_read_cursor;
			IncReadCursor(sizeof(uint8_t));
			return val;
		}
		uint16_t Buffer::ReadShort() {
			if (readRemaining() < sizeof(uint16_t)) return 0;
			uint16_t val = *(uint16_t *)_read_cursor;
			IncReadCursor(sizeof(uint16_t));
			return val;
		}
		uint32_t Buffer::ReadInt() {
			if (readRemaining() < sizeof(uint32_t)) return 0;
			uint32_t val = *(uint32_t *)_read_cursor;
			IncReadCursor(sizeof(uint32_t));
			return val;
		}
		float Buffer::ReadFloat() {
			if (readRemaining() < sizeof(float)) return 0;
			float val = *(float *)_read_cursor;
			IncReadCursor(sizeof(float));
			return val;
		}
		double Buffer::ReadDouble() {
			if (readRemaining() < sizeof(double)) return 0;
			double val = *(double *)_read_cursor;
			IncReadCursor(sizeof(double));
			return val;
		}
		std::string Buffer::ReadNTS() {
			std::string ret;
			if(readRemaining() < 1) return ret;
			char *p = (char *)_read_cursor;
			while (*p && readRemaining() > 0) {
				ret += *p;
				p++;
			}
			IncReadCursor(ret.length() + 1);
			return ret;
		}
		void Buffer::ReadBuffer(void *out, size_t len) {
			if (readRemaining() < len) return;
			memcpy(out, _read_cursor, len);
			IncReadCursor(len);
		}
		/*
			These should all be updated to use WriteBuffer internally
		*/
		void Buffer::WriteByte(uint8_t byte) {
			IncWriteCursor(sizeof(uint8_t));
			*(uint8_t *)_write_cursor_last = byte;
		}
		void Buffer::WriteShort(uint16_t byte) {
			IncWriteCursor(sizeof(uint16_t));
			*(uint16_t *)_write_cursor_last = byte;
		}
		void Buffer::WriteInt(uint32_t byte) {
			IncWriteCursor(sizeof(uint32_t));
			*(uint32_t *)_write_cursor_last = byte;
		}
		void Buffer::WriteFloat(float f) {
			IncWriteCursor(sizeof(float));
			*(float *)_write_cursor_last = f;
		}
		void Buffer::WriteDouble(double d) {
			IncWriteCursor(sizeof(double));
			*(double *)_write_cursor_last = d;
		}
		void Buffer::WriteNTS(std::string str) {
			if (str.length()) {
				int len = str.length();
				IncWriteCursor(len + 1);
				const char *c_str = str.c_str();
				memcpy(_write_cursor_last, c_str, len + 1);
			}
			else {
				WriteByte(0);
			}
		}
		void Buffer::WriteBuffer(void *buf, size_t len) {
			IncWriteCursor(len);
			memcpy(_write_cursor_last, buf, len);
		}
		void Buffer::IncWriteCursor(size_t len) {
			if (bytesWritten()+len >= allocSize()) {
				realloc_buffer(REALLOC_ADD_SIZE+len);
			}

			char *cursor = (char *)_write_cursor;
			_write_cursor_last = cursor;

			cursor += len;
			_write_cursor = cursor;
		}
		void Buffer::IncReadCursor(size_t len) {
			char *cursor = (char *)_read_cursor;
			cursor += len;
			_read_cursor = cursor;
		}
		void *Buffer::GetReadCursor() {
			return _read_cursor;
		}
		void *Buffer::GetWriteCursor() {
			return _write_cursor;
		}
		void *Buffer::GetHead() {
			return mp_ctx->_head;
		}
		void Buffer::resetReadCursor() {
			_read_cursor = mp_ctx->_head;
		}
		void Buffer::resetWriteCursor() {
			_write_cursor = mp_ctx->_head;
			_write_cursor_last = _write_cursor;
		}
		size_t Buffer::allocSize() {
			return mp_ctx->alloc_size;
		}
		size_t Buffer::readRemaining() {
			size_t end = bytesWritten();
			size_t diff = ((size_t)mp_ctx->_head - (size_t)_read_cursor) + end;
			return diff;
		}
		size_t Buffer::bytesWritten() {
			size_t size = (size_t)_write_cursor - (size_t)mp_ctx->_head;
			return size;
		}
		void Buffer::realloc_buffer(size_t new_size) {
			if (!mp_ctx->pointer_owner) return;
			int write_offset = bytesWritten();
			int read_offset = readRemaining();
			mp_ctx->_head = realloc(mp_ctx->_head, write_offset + new_size);
			mp_ctx->alloc_size = write_offset + new_size;
			resetCursors();
			SetWriteCursor(write_offset);
			SetReadCursor(read_offset);
		}
		void Buffer::SetReadCursor(size_t len) {
			if (len > mp_ctx->alloc_size)
				return;
			_read_cursor = (void *)((size_t)mp_ctx->_head + (size_t)len);
		}
		void Buffer::SetWriteCursor(size_t len) {
			if (len > mp_ctx->alloc_size)
				return;
			_write_cursor = (void *)((size_t)mp_ctx->_head + (size_t)len);
		}
		void Buffer::resetCursors() {
			_write_cursor = mp_ctx->_head;
			_write_cursor_last = mp_ctx->_head;
			_read_cursor = mp_ctx->_head;
		}
}
