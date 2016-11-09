/* chc_endian by Andrew "CHCNiZ" Learn 
 * Copyright (C) 2009 Andrew Learn
 *
 *  chc_endian is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  chc_endian is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with chc_endian.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "chc_endian.h"
//now obviously madness like this isn't used often(who would need invert_bytes for example) a lot of its just for fun and no cpu i've heard of uses such a byte order, but its still included into this header
uint32_t little_to_middle(uint32_t x) { //0xABCDEF02 becomes 0xCDAB02EF this is used for converting from middle endian(ARM CPU's/PDP-11) to little endian
return x<<8&0xff000000|x>>8&0xff0000|(x&0x0000ff00)>>8|(x&0x000000ff)<<8;
}
uint32_t reverse_endian32(uint32_t x) { //little to big or vice versa
return	(x<<24|(x<<8&0x00ff0000)|x>>8&0x0000ff00|x>>24&0x000000ff);
}
uint32_t invert_bytes(uint32_t x) { //0xABCDEF02 becomes 0xBADCFE20
return ((x&0x0f000000)<<4|(x&0xf0000000)>>4|(x&0x00f00000)>>4|(x&0x000f0000)<<4|(x&0x0000f000)>>4|(x&0x00000f00)<<4|(x&0x000000f0)>>4|(x&0x0000000f)<<4);
}
uint32_t rev_bendian32(uint32_t x) { //0xABCDEF02 becomes 0x20FEDCAB
return ((x&0x0f0000)<<4|(x&0xf00000)>>4)>>8|((x&0x0f)<<4|(x&0xf0)>>4)<<24|((x&0x0f00)<<4|(x&0xf000)>>4)<<8|((x&0x0f000000)<<4|(x&0xf0000000)>>4)>>24;
}
uint16_t reverse_endian16(uint16_t x) { //little to big or vice versa
return (x&0xff00)>>8|(x&0x00ff)<<8;
}
uint16_t invert_inner16(uint16_t x) {//0xABCD becomes 0xACBD
return x&0xf00f|(x&0x0f00)>>4|(x&0x00f0)<<4;
}
uint16_t invert_outer16(uint16_t x) { //0xABCD becomes 0xDBCA
return (x&0x000f)<<12|x&0x0ff0|(x&0xf000)>>12;
}
uint16_t flipbyte16(uint16_t x) { //0xABCD becomes 0xBADC
return (x&0x0f00)<<4|(x&0xf000)>>4|(x&0x000f)<<4|(x&0x00f0)>>4;
}
uint8_t flipbyte(uint8_t x) { //0xAB becomes 0xBA
return x<<4&0xf0|(x>>4)&0x0f;
}
