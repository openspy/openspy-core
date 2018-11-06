#include <test/common.h>
#include <OS/Buffer.h>
void test_buffer_overwrite_memory_buffer() {
    OS::Buffer buffer(1);
    char test_buff_len = 999256;
    char *test_buff = (char *)malloc(test_buff_len);
    for(int i=0;i<test_buff_len;i++)  {
        test_buff[i] = rand();
    }
    buffer.WriteBuffer(&test_buff,sizeof(test_buff)*2);
    free((void *)test_buff);
}
void test_buffer_overread_buffer() {
    OS::Buffer buffer(1);
    char test_buff_len = 999256;
    char *test_buff = (char *)malloc(test_buff_len);
    buffer.WriteByte(55);
    buffer.ReadBuffer(test_buff, test_buff_len);
    for(int i=0;i<test_buff_len*2;i++) {
        buffer.ReadByte();
        buffer.ReadNTS();
    }
    free((void *)test_buff);
}
int main() {
    test_buffer_overwrite_memory_buffer();
    test_buffer_overread_buffer();
    return 0;
}