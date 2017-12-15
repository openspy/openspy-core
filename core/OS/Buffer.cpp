/*
	This buffer class manages itself to prevent memory leaks, and auto-reallocs when needed, but is not thread safe.
*/
#include "Buffer.h"
#define REALLOC_ADD_SIZE 512
#define BUFFER_SAFE_SIZE 128 //if this amount of bytes isn't available, realloc
namespace OS {
		BufferCtx::BufferCtx(int alloc_size) : OS::Ref() {
			_head = malloc(alloc_size);
			_cursor = _head;
			pointer_owner = true;
			this->alloc_size = alloc_size;
		}
		BufferCtx::BufferCtx(void *addr, int len) : OS::Ref() {
			pointer_owner = false; 
			_head = addr;
			_cursor = _head;
			alloc_size = len;
		}
		BufferCtx::~BufferCtx() {
			if (pointer_owner) {
				free((void *)_head);
			}
		}
		Buffer::Buffer(void *addr, int len) {
			mp_ctx = new BufferCtx(addr, len);
		}
		Buffer::Buffer(int alloc_size) {
			mp_ctx = new BufferCtx(alloc_size);
		}	
		Buffer::Buffer(Buffer &cpy) {
			cpy.mp_ctx->IncRef();
			mp_ctx = cpy.mp_ctx;
		}
		Buffer::Buffer() {
			mp_ctx = new BufferCtx(1024 * 1024);
		}
		Buffer::~Buffer() {
			mp_ctx->DecRef();
			if (mp_ctx->GetRefCount() == 0) {
				delete mp_ctx;
			}
		}	
		uint8_t Buffer::ReadByte() {
			if (remaining() < sizeof(uint8_t)) return 0;
			uint8_t val = *(uint8_t *)mp_ctx->_cursor;
			IncCursor(sizeof(uint8_t));
			return val;
		}
		uint16_t Buffer::ReadShort() {
			if (remaining() < sizeof(uint16_t)) return 0;
			uint16_t val = *(uint16_t *)mp_ctx->_cursor;
			IncCursor(sizeof(uint16_t));
			return val;
		}
		uint32_t Buffer::ReadInt() {
			if (remaining() < sizeof(uint32_t)) return 0;
			uint32_t val = *(uint32_t *)mp_ctx->_cursor;
			IncCursor(sizeof(uint32_t));
			return val;
		}
		float Buffer::ReadFloat() {
			if (remaining() < sizeof(float)) return 0;
			float val = *(float *)mp_ctx->_cursor;
			IncCursor(sizeof(float));
			return val;
		}
		double Buffer::ReadDouble() {
			if (remaining() < sizeof(double)) return 0;
			double val = *(double *)mp_ctx->_cursor;
			IncCursor(sizeof(double));
			return val;
		}
		std::string Buffer::ReadNTS() {
			std::string ret;
			char *p = (char *)mp_ctx->_cursor;
			while (*p && remaining() > 0) {
				ret += *p;
				p++;
			}
			IncCursor(ret.length() + 1);
			return ret;
		}
		void Buffer::ReadBuffer(void *out, int len) {
			if (remaining() < len) return;
			memcpy(out, mp_ctx->_cursor, len);
			IncCursor(len);
		}
		void Buffer::WriteByte(uint8_t byte) {
			*(uint8_t *)mp_ctx->_cursor = byte;
			IncCursor(sizeof(uint8_t), true);
		}
		void Buffer::WriteShort(uint16_t byte) {
			*(uint16_t *)mp_ctx->_cursor = byte;
			IncCursor(sizeof(uint16_t), true);
		}
		void Buffer::WriteInt(uint32_t byte) {
			*(uint32_t *)mp_ctx->_cursor = byte;
			IncCursor(sizeof(uint32_t), true);
		}
		void Buffer::WriteFloat(float f) {
			*(float *)mp_ctx->_cursor = f;
			IncCursor(sizeof(float), true);
		}
		void Buffer::WriteDouble(double d) {
			*(double *)mp_ctx->_cursor = d;
			IncCursor(sizeof(double), true);
		}
		void Buffer::WriteNTS(std::string str) {
			if (str.length() > remaining()) {
				realloc_buffer(str.length() + REALLOC_ADD_SIZE);
			}
			if (str.length()) {
				int len = str.length();
				const char *c_str = str.c_str();
				memcpy(mp_ctx->_cursor, c_str, len + 1);
				IncCursor(len + 1, true);
			}
			else {
				WriteByte(0);
			}
		}
		void Buffer::WriteBuffer(void *buf, int len) {
			if (len > remaining()) {
				realloc_buffer(len + REALLOC_ADD_SIZE);
			}
			memcpy(mp_ctx->_cursor, buf, len);
			IncCursor(len, true);
		}
		void Buffer::IncCursor(int len, bool write_operation) {
			char *cursor = (char *)mp_ctx->_cursor;
			cursor += len;
			mp_ctx->_cursor = cursor;

			if (write_operation && remaining() < BUFFER_SAFE_SIZE) {
				realloc_buffer(REALLOC_ADD_SIZE);
			}
		}
		void *Buffer::GetCursor() {
			return mp_ctx->_cursor;
		}
		void *Buffer::GetHead() {
			return mp_ctx->_head;
		}
		void Buffer::reset() {
			mp_ctx->_cursor = mp_ctx->_head;
		}
		int Buffer::remaining() {
			size_t end = (size_t)mp_ctx->_head + (size_t)mp_ctx->alloc_size;
			size_t diff = end - (size_t)mp_ctx->_cursor;
			return diff;
		}
		int Buffer::size() {
			size_t size = (size_t)mp_ctx->_cursor - (size_t)mp_ctx->_head;
			return size;
		}
		void Buffer::realloc_buffer(int new_size) {
			if (!mp_ctx->pointer_owner) return;
			int offset = remaining();
			mp_ctx->_head = realloc(mp_ctx->_head, size() + new_size);
			reset();
			IncCursor(offset);
		}
}
