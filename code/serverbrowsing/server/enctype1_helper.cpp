#include <OS/Buffer.h>

void write_enctype1_encrypted_data(OS::Buffer &output, const char *validate_key, unsigned char *scramble_data, int scrambled_len, char *encrypted_data, int encrypted_len, uint32_t encshare4_data);

static const unsigned char enctype1_master_key[] = /* pre-built */
    "\x01\xba\xfa\xb2\x51\x00\x54\x80\x75\x16\x8e\x8e\x02\x08\x36\xa5"
    "\x2d\x05\x0d\x16\x52\x07\xb4\x22\x8c\xe9\x09\xd6\xb9\x26\x00\x04"
    "\x06\x05\x00\x13\x18\xc4\x1e\x5b\x1d\x76\x74\xfc\x50\x51\x06\x16"
    "\x00\x51\x28\x00\x04\x0a\x29\x78\x51\x00\x01\x11\x52\x16\x06\x4a"
    "\x20\x84\x01\xa2\x1e\x16\x47\x16\x32\x51\x9a\xc4\x03\x2a\x73\xe1"
    "\x2d\x4f\x18\x4b\x93\x4c\x0f\x39\x0a\x00\x04\xc0\x12\x0c\x9a\x5e"
    "\x02\xb3\x18\xb8\x07\x0c\xcd\x21\x05\xc0\xa9\x41\x43\x04\x3c\x52"
    "\x75\xec\x98\x80\x1d\x08\x02\x1d\x58\x84\x01\x4e\x3b\x6a\x53\x7a"
    "\x55\x56\x57\x1e\x7f\xec\xb8\xad\x00\x70\x1f\x82\xd8\xfc\x97\x8b"
    "\xf0\x83\xfe\x0e\x76\x03\xbe\x39\x29\x77\x30\xe0\x2b\xff\xb7\x9e"
    "\x01\x04\xf8\x01\x0e\xe8\x53\xff\x94\x0c\xb2\x45\x9e\x0a\xc7\x06"
    "\x18\x01\x64\xb0\x03\x98\x01\xeb\x02\xb0\x01\xb4\x12\x49\x07\x1f"
    "\x5f\x5e\x5d\xa0\x4f\x5b\xa0\x5a\x59\x58\xcf\x52\x54\xd0\xb8\x34"
    "\x02\xfc\x0e\x42\x29\xb8\xda\x00\xba\xb1\xf0\x12\xfd\x23\xae\xb6"
    "\x45\xa9\xbb\x06\xb8\x88\x14\x24\xa9\x00\x14\xcb\x24\x12\xae\xcc"
    "\x57\x56\xee\xfd\x08\x30\xd9\xfd\x8b\x3e\x0a\x84\x46\xfa\x77\xb8";

void enctype1_func3(unsigned char *, int, unsigned char *);
void enctype1_func2(unsigned char *data, int size, unsigned char *key);
void enctype1_func4(unsigned char *, int, unsigned char *);
void enctype1_func8(unsigned char *, int, const unsigned char *);
void enctype1_func6e(unsigned char *, int, unsigned char *);
int  enctype1_func7e(int, unsigned char *);
void encshare1(unsigned int *tbuff, unsigned char *datap, int len);
void encshare4(unsigned char *src, int size, unsigned int *dest);

void obfuscate_scramble_data(unsigned char *scramble_data, int scramble_len, const char *master_key, int master_key_len);
int find_master_key_offset(char key, const char *master_key, int master_key_len);


void create_enctype1_buffer(const char *validate_key, OS::Buffer input, OS::Buffer &output) {
    char *encrypt_data = input.GetHead();
    int encrypt_len = input.bytesWritten();

    int encshare4_data = rand();
    int tmplen = (encrypt_len >> 1) - 17;
    unsigned int    tbuff[326];
    unsigned char  enc1key[261];
    memset(&tbuff,0,sizeof(tbuff));
    if(tmplen >= 0) {
        encshare4((char *)&encshare4_data, sizeof(encshare4_data), tbuff);
        encshare1(tbuff, encrypt_data, tmplen);
    }

    char scramble_data[16];
    for(int i=0;i<sizeof(scramble_data);i++) {
        uint8_t key = rand();
        scramble_data[i] = enctype1_master_key[key];
    }


    char encryption_key[258];
    enctype1_func3(scramble_data, sizeof(scramble_data), encryption_key);    
    enctype1_func2((char *)encrypt_data, encrypt_len, (char *)&encryption_key); //encrypt actual data

    tmplen = (encrypt_len >> 2) - 5;
    if(tmplen >= 0) {
        enctype1_func4(validate_key, strlen(validate_key), enc1key);
        enctype1_func6e(encrypt_data, tmplen, enc1key);
    }
    obfuscate_scramble_data(scramble_data, sizeof(scramble_data), enctype1_master_key, sizeof(enctype1_master_key)); 

    write_enctype1_encrypted_data(output, validate_key, scramble_data, sizeof(scramble_data), (char *)encrypt_data, encrypt_len, encshare4_data);

}

int find_master_key_offset(char key, const char *master_key, int master_key_len) {
    for(int i=0;i<master_key_len;i++) {
        if(master_key[i] == key) {
            return i;
        }
    }
    return -1;
}

/*
This routine will only return the first instance in the master key... so it is not really secure
*/
void obfuscate_scramble_data(unsigned char *scramble_data, int scramble_len, const char *master_key, int master_key_len) {
    for(int i=0;i<scramble_len;i++) {
        int offset =  find_master_key_offset(scramble_data[i], master_key, master_key_len);
        if(offset != -1) {
            scramble_data[i] = offset;
        }
    }
}

void write_enctype1_encrypted_data(OS::Buffer &output, const char *validate_key, unsigned char *scramble_data, int scrambled_len, char *encrypted_data, int encrypted_len, uint32_t encshare4_data) {
    output.WriteInt(0); //total length - written at end
    output.WriteByte(42); //(42 ^ 62) - 20;
    output.WriteByte(218); //(218 ^ 205) - 5;

    for(int i=0;i<13;i++) { //unused data...?
        output.WriteByte(rand()); 
    }
    output.WriteBuffer(scramble_data, scrambled_len);
    output.WriteByte(0); //unknown data
    output.WriteInt(encshare4_data);

    for(int i=0;i<18;i++) { //unused data...?
        output.WriteByte(rand()); 
    }

    output.WriteBuffer(encrypted_data, encrypted_len);

    int total_len = output.bytesWritten();
    int *len_address = (int *)output.GetHead();
    *len_address = htonl(total_len);

}