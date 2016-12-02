import binascii
from M2Crypto import RSA, BIO
priv_key = open("private_key.pem", "rb")


def get_pass(*args):
	return str('123321')
bio = BIO.MemoryBuffer(priv_key.read())
rsa = RSA.load_key_bio(bio, get_pass)
n, e = binascii.hexlify(rsa.n), binascii.hexlify(rsa.e)
print("N: {}\nE: {}\n".format(n,e))
encrypted = rsa.public_encrypt('test123', RSA.pkcs1_padding)
encoded = binascii.hexlify(encrypted)

print("Encoded: {}\n".format(encoded))

encrypted_hex = "9d9c19b21b2a8d3c7229b295aec4729d23e97f8030d106f49358bad64cb20648e5560578e84de2c212a2338e3ce14baaf019a9fcb0d650837498fdb494232ef4ae221996b9afcee55ba27343dc80df8dd45aa6263dbdd7e62c0179d24d30c249f3841276651f2eee6666d2c29c6c92741f0f0792e18fedd24ebc555bdb2f56b8"
encrypted = binascii.unhexlify(encrypted_hex)

decrypted = rsa.private_decrypt(encrypted, RSA.pkcs1_padding)
print("Decoded: {}\n".format(decrypted))