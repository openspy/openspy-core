#include <OS/Buffer.h>

/*
despite being a stream cipher, gamespy seems to require enctype1 to be sent out as 1 continuous buffer, solely because the length is sent as the first 4 bytes

its posible this is only required in the aluigi implementation, and not actually in gamespy 3d, however for now its a continuous buffer for this reason

*/

void create_enctype1_buffer(const char *validate_key, OS::Buffer input, OS::Buffer &output);