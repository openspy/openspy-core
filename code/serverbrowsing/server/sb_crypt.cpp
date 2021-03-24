///////////////////////////////////////////////////////////////////////////////
// File:	sb_crypt.c
// SDK:		GameSpy Server Browsing SDK
//
// Copyright Notice: This file is part of the GameSpy SDK designed and 
// developed by GameSpy Industries. Copyright (c) 2009 GameSpy Industries, Inc.

#include <stdlib.h>
#include <string.h>
#include "sb_crypt.h"

static unsigned char keyrand(GOACryptState *state,
							 unsigned limit,
							 unsigned mask,
							 unsigned char *user_key,
							 unsigned char keysize,
							 unsigned char *rsum,
							 unsigned *keypos)
{
    unsigned int u,         // Value from 0 to limit to return.
        retry_limiter;      // No infinite loops allowed.
	
    if (!limit) return 0;   // Avoid divide by zero error.

    retry_limiter = 0;
    do
	{
        *rsum = (unsigned char)(state->cards[*rsum] + user_key[(*keypos)++]);
        if (*keypos >= keysize)
		{
            *keypos = 0;            // Recycle the user key.
            *rsum = (unsigned char)(*rsum + keysize);   // key "aaaa" != key "aaaaaaaa"
		}
        u = mask & *rsum;
        if (++retry_limiter > 11)
            u %= limit;     // Prevent very rare long loops.
	}
    while (u > limit);
    return (unsigned char)(u);
}

static const unsigned char cards_ascending[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
	0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
	0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
	0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

#define SWAP(x,y,swaptemp) (swaptemp) = (x); (x) = (y); (y) = (swaptemp)

void GOAHashInit(GOACryptState *state)
{
	// This function is used to initialize non-keyed hash
	// computation.

	int i, j;

	// Initialize the indices and data dependencies.

	state->rotor = 1;
	state->ratchet = 3;
	state->avalanche = 5;
	state->last_plain = 7;
	state->last_cipher = 11;

	// Start with state->cards all in inverse order.

	for (i=0,j=255; i<256; i++,j--)
		state->cards[i] = (unsigned char) j;
}

void GOACryptInit(GOACryptState *state, unsigned char *key, unsigned char keysize)
{
    // Key size may be up to 256 bytes.
    // Pass phrases may be used directly, with longer length
    // compensating for the low entropy expected in such keys.
    // Alternatively, shorter keys hashed from a pass phrase or
    // generated randomly may be used. For random keys, lengths
    // of from 4 to 16 bytes are recommended, depending on how
    // secure you want this to be.
	
    unsigned int i;
    unsigned char toswap, swaptemp, rsum;
    unsigned keypos, mask;
	
    // If we have been given no key, assume the default hash setup.
	
    if (keysize < 1)
	{
        GOAHashInit(state);
        return;
	}
	
    // Start with state->cards all in order, one of each.
	memcpy(state->cards, cards_ascending, sizeof(cards_ascending));
	
    // Swap the card at each position with some other card.
    toswap = 0;
    keypos = 0;         // Start with first byte of user key.
    rsum = 0;
	mask = 255;
	// FM: Original code did "i>=0", but when i==0 the statements inside the loop body have no effect.
    for (i=255;i>0;i--)
	{
        toswap = keyrand(state, i, mask, key, keysize, &rsum, &keypos);
		SWAP(state->cards[i], state->cards[toswap], swaptemp);
		if ((i & (i - 1)) == 0)
			mask >>= 1;
	}
	
    // Initialize the indices and data dependencies.
    // Indices are set to different values instead of all 0
    // to reduce what is known about the state of the state->cards
    // when the first byte is emitted.
	
    state->rotor = state->cards[1];
    state->ratchet = state->cards[3];
    state->avalanche = state->cards[5];
    state->last_plain = state->cards[7];
    state->last_cipher = state->cards[rsum];
	
    toswap = swaptemp = rsum = 0;
    keypos = 0;
}

unsigned char GOAEncryptByte(GOACryptState *state, unsigned char byte)
{
    // Picture a single enigma rotor with 256 positions, rewired
    // on the fly by card-shuffling.
	
    // This cipher is a variant of one invented and written
    // by Michael Paul Johnson in November, 1993.
	
    unsigned char swaptemp;
	
    // Shuffle the deck a little more.
	state->ratchet = (unsigned char)(state->ratchet + state->cards[state->rotor++]);
    swaptemp = state->cards[state->last_cipher];
    state->cards[state->last_cipher] = state->cards[state->ratchet];
    state->cards[state->ratchet] = state->cards[state->last_plain];
    state->cards[state->last_plain] = state->cards[state->rotor];
    state->cards[state->rotor] = swaptemp;
    state->avalanche = (unsigned char )(state->avalanche + state->cards[swaptemp]);
	
    // Output one byte from the state in such a way as to make it
    // very hard to figure out which one you are looking at.
	/*
    state->last_cipher = b^state->state->cards[(state->state->cards[state->ratchet] + state->state->cards[state->rotor]) & 0xFF] ^
	state->state->cards[state->state->cards[(state->state->cards[state->last_plain] +
	state->state->cards[state->last_cipher] +
	state->state->cards[state->avalanche])&0xFF]];
	*/
    state->last_cipher = (unsigned char)(byte ^
		state->cards[(state->cards[state->avalanche] + state->cards[state->rotor]) & 0xFF] ^
		state->cards[state->cards[(state->cards[state->last_plain] + state->cards[state->last_cipher] + state->cards[state->ratchet]) & 0xFF]]);
    state->last_plain = byte;
    return state->last_cipher;
}

void GOAEncrypt(GOACryptState *state, unsigned char *bp, size_t len)
{
	unsigned char Rotor = state->rotor;
	unsigned char Ratchet = state->ratchet;
	unsigned char Avalanche = state->avalanche;
	unsigned char Last_plain = state->last_plain;
	unsigned char Last_cipher = state->last_cipher;
	unsigned char swaptemp;
	size_t i;

	for (i=0; i<len; i++) {
		Ratchet = (unsigned char)(Ratchet + state->cards[Rotor++]);
		swaptemp = state->cards[Last_cipher];
		state->cards[Last_cipher] = state->cards[Ratchet];
		state->cards[Ratchet] = state->cards[Last_plain];
		state->cards[Last_plain] = state->cards[Rotor];
		state->cards[Rotor] = swaptemp;
		Avalanche = (unsigned char)(Avalanche + state->cards[swaptemp]);

		Last_cipher = (unsigned char)(bp[i] ^
			state->cards[(state->cards[Avalanche] + swaptemp) & 0xFF] ^
			state->cards[state->cards[(state->cards[Last_plain] + state->cards[Last_cipher] + state->cards[Ratchet]) & 0xFF]]);
		Last_plain = bp[i];
		bp[i] = Last_cipher;
	}

	state->rotor = Rotor;
	state->ratchet = Ratchet;
	state->avalanche = Avalanche;
	state->last_plain = Last_plain;
	state->last_cipher = Last_cipher;
}

unsigned char GOADecryptByte(GOACryptState *state, unsigned char byte)
{
    unsigned char swaptemp;
	
    // Shuffle the deck a little more.
    state->ratchet = (unsigned char)(state->ratchet + state->cards[state->rotor++]);
    swaptemp = state->cards[state->last_cipher];
    state->cards[state->last_cipher] = state->cards[state->ratchet];
    state->cards[state->ratchet] = state->cards[state->last_plain];
    state->cards[state->last_plain] = state->cards[state->rotor];
    state->cards[state->rotor] = swaptemp;
    state->avalanche = (unsigned char)(state->avalanche + state->cards[swaptemp]);
	
    // Output one byte from the state in such a way as to make it
    // very hard to figure out which one you are looking at.
	/*
    state->last_plain = b^state->cards[(state->cards[state->ratchet] + state->cards[state->rotor]) & 0xFF] ^
	state->cards[state->cards[(state->cards[state->last_plain] +
	state->cards[state->last_cipher] +
	state->cards[state->avalanche])&0xFF]];
	*/
	//crt - change this around
	state->last_plain = (unsigned char)(byte ^
		state->cards[(state->cards[state->avalanche] + state->cards[state->rotor]) & 0xFF] ^
		state->cards[state->cards[(state->cards[state->last_plain] + state->cards[state->last_cipher] + state->cards[state->ratchet])&0xFF]]);
    state->last_cipher = byte;
    return state->last_plain;
}

void GOADecrypt(GOACryptState *state, unsigned char *bp, size_t len)
{
	unsigned char Rotor = state->rotor;
	unsigned char Ratchet = state->ratchet;
	unsigned char Avalanche = state->avalanche;
	unsigned char Last_plain = state->last_plain;
	unsigned char Last_cipher = state->last_cipher;
	unsigned char swaptemp;
	size_t i;

	for (i=0; i<len; i++) {
		Ratchet = (unsigned char)(Ratchet + state->cards[Rotor++]);
		swaptemp = state->cards[Last_cipher];
		state->cards[Last_cipher] = state->cards[Ratchet];
		state->cards[Ratchet] = state->cards[Last_plain];
		state->cards[Last_plain] = state->cards[Rotor];
		state->cards[Rotor] = swaptemp;
		Avalanche = (unsigned char)(Avalanche + state->cards[swaptemp]);

		// Output one byte from the state in such a way as to make it
		// very hard to figure out which one you are looking at.
		/*
		state->last_plain = b^state->cards[(state->cards[state->ratchet] + state->cards[state->rotor]) & 0xFF] ^
		state->cards[state->cards[(state->cards[state->last_plain] +
		state->cards[state->last_cipher] +
		state->cards[state->avalanche])&0xFF]];
		*/
		//crt - change this around
		Last_plain = (unsigned char)(bp[i] ^
			state->cards[(state->cards[Avalanche] + state->cards[Rotor]) & 0xFF] ^
			state->cards[state->cards[(state->cards[Last_plain] + state->cards[Last_cipher] + state->cards[Ratchet])&0xFF]]);
		Last_cipher = bp[i];
		bp[i] = Last_plain;
	}

	state->rotor = Rotor;
	state->ratchet = Ratchet;
	state->avalanche = Avalanche;
	state->last_plain = Last_plain;
	state->last_cipher = Last_cipher;
}

void GOAHashFinal(GOACryptState *state, unsigned char *outputhash,      // Destination
				  unsigned char hashlength) // Size of hash.
{
    int i;
	
    for (i=255;i>=0;i--)
        GOAEncryptByte(state, (unsigned char) i);
    for (i=0;i<hashlength;i++)
        outputhash[i] = GOAEncryptByte(state, 0);
}
