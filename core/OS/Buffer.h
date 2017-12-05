#ifndef _OS_BUFFER_H
#define _OS_BUFFER_H
#include <stdint.h>
#include <string>
namespace OS {
	class Buffer {
		public:
			Buffer(Buffer &cpy);
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
		private:
			void IncCursor(int len);
			void *_head;
			void *_cursor;
			int alloc_size;
			bool pointer_owner;
	};
}
#endif //_OS_BUFFER_H