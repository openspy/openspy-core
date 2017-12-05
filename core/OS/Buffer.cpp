#include "Buffer.h"
namespace OS {
		Buffer::Buffer(void *addr, int len) {
			_head = addr;
			_cursor = addr;
			alloc_size = len;
			pointer_owner = false;
		}
		Buffer::Buffer(int alloc_size) {
		}	
		Buffer::Buffer(Buffer &cpy) {
			_head = cpy._head;
			_cursor = cpy._cursor;
			pointer_owner = false;
			alloc_size = cpy.alloc_size;
		}
		Buffer::Buffer() {
			alloc_size = 1024 * 1024;
			_head = malloc(alloc_size);
			_cursor = _head;
			pointer_owner = true;
		}
		Buffer::~Buffer() {
			if (pointer_owner) {
				free((void *)_head);
			}
		}	
		uint8_t Buffer::ReadByte() {
			uint8_t val = *(uint8_t *)_cursor;
			IncCursor(sizeof(uint8_t));
			return val;
		}
		uint16_t Buffer::ReadShort() {
			uint16_t val = *(uint16_t *)_cursor;
			IncCursor(sizeof(uint16_t));
			return val;
		}
		uint32_t Buffer::ReadInt() {
			uint32_t val = *(uint32_t *)_cursor;
			IncCursor(sizeof(uint32_t));
			return val;
		}
		float Buffer::ReadFloat() {
			float val = *(float *)_cursor;
			IncCursor(sizeof(float));
			return val;
		}
		double Buffer::ReadDouble() {
			double val = *(double *)_cursor;
			IncCursor(sizeof(double));
			return val;
		}
		std::string Buffer::ReadNTS() {
			std::string ret;
			char *p = (char *)_cursor;
			while (*p) {
				ret += *p;
				p++;
			}
			IncCursor(ret.length() + 1);
			return ret;
		}
		void Buffer::ReadBuffer(void *out, int len) {
			memcpy(out, _cursor, len);
			IncCursor(len);
		}
		void Buffer::WriteByte(uint8_t byte) {
			*(uint8_t *)_cursor = byte;
			IncCursor(sizeof(uint8_t));
		}
		void Buffer::WriteShort(uint16_t byte) {
			*(uint16_t *)_cursor = byte;
			IncCursor(sizeof(uint16_t));
		}
		void Buffer::WriteInt(uint32_t byte) {
			*(uint32_t *)_cursor = byte;
			IncCursor(sizeof(uint32_t));
		}
		void Buffer::WriteFloat(float f) {
			*(float *)_cursor = f;
			IncCursor(sizeof(float));
		}
		void Buffer::WriteDouble(double d) {
			*(double *)_cursor = d;
			IncCursor(sizeof(double));
		}
		void Buffer::WriteNTS(std::string str) {
			if (str.length()) {
				int len = str.length();
				const char *c_str = str.c_str();
				memcpy(_cursor, c_str, len + 1);
				IncCursor(len + 1);
			}
			else {
				WriteByte(0);
			}
		}
		void Buffer::WriteBuffer(void *buf, int len) {
			memcpy(_cursor, buf, len);
			IncCursor(len);
		}
		void Buffer::IncCursor(int len) {
			char *cursor = (char *)_cursor;
			cursor += len;
			_cursor = cursor;
		}
		void *Buffer::GetCursor() {
			return _cursor;
		}
		void *Buffer::GetHead() {
			return _head;
		}
		void Buffer::reset() {
			_cursor = _head;
		}
		int Buffer::remaining() {
			size_t end = (size_t)_head + (size_t)alloc_size;
			size_t diff = end - (size_t)_cursor;
			return diff;
		}
		int Buffer::size() {
			size_t size = (size_t)_cursor - (size_t)_head;
			return size;
		}
}
