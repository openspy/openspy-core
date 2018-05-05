#ifndef _OS_BUFFER_H
#define _OS_BUFFER_H
#include <stdint.h>
#include <string>
#include <OS/OpenSpy.h>
#include <OS/Ref.h>
namespace OS {
	class BufferCtx : public OS::Ref {
	private:
		BufferCtx(int alloc_size);
		BufferCtx(void *addr, int len);
		~BufferCtx();
		void *_head;
		int alloc_size;
		bool pointer_owner;
		friend class Buffer;
	};
	class Buffer {
		public:
			Buffer(const Buffer &cpy);
			Buffer(void *addr, int len);
			Buffer(int alloc_size);
			Buffer();
			~Buffer();
			uint8_t ReadByte();
			uint16_t ReadShort();
			uint32_t ReadInt();
			float ReadFloat();
			double ReadDouble();
			std::string ReadNTS();
			void ReadBuffer(void *out, int len);
		
			void WriteByte(uint8_t byte);
			void WriteShort(uint16_t byte);
			void WriteInt(uint32_t byte);
			void WriteFloat(float f);
			void WriteDouble(double d);
			void WriteNTS(std::string str);
			void WriteBuffer(void *buf, int len);
			int remaining();
			void *GetCursor();
			void *GetHead();
			void reset();
			int size();
			void SetCursor(int pos);

			Buffer &operator=(const Buffer& val) {
				if(mp_ctx) {
					mp_ctx->DecRef();
					if (mp_ctx->GetRefCount() == 0) {
						delete mp_ctx;
					}
				}
				
				mp_ctx = val.mp_ctx;
				_cursor = val._cursor;
				mp_ctx->IncRef();
				return *this;
			}
		private:
			void realloc_buffer(int new_size);
			void IncCursor(int len, bool write_operation = false);
			BufferCtx *mp_ctx;
			void *_cursor;
	};
}
#endif //_OS_BUFFER_H