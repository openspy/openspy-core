#ifndef _OS_BUFFER_H
#define _OS_BUFFER_H
#include <stdint.h>
#include <string>
#include <OS/OpenSpy.h>
#include <OS/Ref.h>
namespace OS {
	class BufferCtx : public OS::Ref {
	private:
		BufferCtx(size_t alloc_size);
		BufferCtx(void *addr, size_t len);
		~BufferCtx();
		void *_head;
		size_t alloc_size;
		bool pointer_owner;
		friend class Buffer;
	};
	class Buffer {
		public:
			Buffer(const Buffer &cpy);
			Buffer(void *addr, size_t len);
			Buffer(size_t alloc_size);
			Buffer();
			~Buffer();
			uint8_t ReadByte();
			uint16_t ReadShort();
			uint32_t ReadInt();
			uint64_t ReadLong();
			float ReadFloat();
			double ReadDouble();
			std::string ReadNTS();
			void ReadBuffer(void *out, size_t len);
		
			void WriteByte(uint8_t byte);
			void WriteShort(uint16_t byte);
			void WriteLong(uint64_t value);
			void WriteInt(uint32_t byte);
			void WriteFloat(float f);
			void WriteDouble(double d);
			void WriteNTS(std::string str);
			void WriteBuffer(const void *buf, size_t len);

			void SkipRead(size_t len) { IncReadCursor(len); }

			size_t readRemaining();
			size_t bytesWritten();
			size_t allocSize();

			void *GetReadCursor();
			void *GetWriteCursor();

			void resetCursors();
			void resetReadCursor();
			void resetWriteCursor();

			void SetReadCursor(size_t pos);
			void SetWriteCursor(size_t pos);

			void *GetHead();

			Buffer &operator=(const Buffer& val) {
				if(mp_ctx) {
					if (mp_ctx->DecRef() == 0) {
						delete mp_ctx;
					}
				}
				
				mp_ctx = val.mp_ctx;
				_read_cursor = val._read_cursor;
				_write_cursor = val._write_cursor;
				mp_ctx->IncRef();
				return *this;
			}
		private:
			void realloc_buffer(size_t new_size);
			void IncReadCursor(size_t len);
			void IncWriteCursor(size_t len);
			BufferCtx *mp_ctx;
			void *_read_cursor;
			void *_write_cursor;
			void *_write_cursor_last;
	};
}
#endif //_OS_BUFFER_H