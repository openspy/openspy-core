int func7e(char v, unsigned char *enc1key) {
    unsigned char   a,
                    b,
                    c;
    a = enc1key[256];
    b = enc1key[257];
    c = enc1key[a];
    enc1key[256] = a + 1;
    enc1key[257] = b + c;
    a = enc1key[260];
    b = enc1key[257];
    b = enc1key[b];
    c = enc1key[a];
    enc1key[a] = b;
    a = enc1key[259];
    b = enc1key[257];
    a = enc1key[a];
    enc1key[b] = a;
    a = enc1key[256];
    b = enc1key[259];
    a = enc1key[a];
    enc1key[b] = a;
    a = enc1key[256];
    enc1key[a] = c;
    b = enc1key[258];
    a = enc1key[c];
    c = enc1key[259];
    b = b + a;
    enc1key[258] = b;
    a = b;   

    c = enc1key[c];
    b = enc1key[257];
    b = enc1key[b];
    a = enc1key[a];
    c += b;
    b = enc1key[260];
    b = enc1key[b];
    c += b;  

    b = enc1key[c];
    c = enc1key[256];
    c = enc1key[c];
    a += c;

    c = enc1key[b];
    b = enc1key[a]; 

    c ^= b;
    c ^= v;

    enc1key[260] = c;
    enc1key[259] = v;
    return c;
}


void func6e(unsigned char *data, int len, unsigned char *enc1key) {
    while(len--) {
        *data = func7e(*data, enc1key);
        data++;
    }
}
