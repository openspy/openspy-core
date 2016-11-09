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
#ifndef _CHC_ENDIAN_INC
#define _CHC_ENDIAN_INC
#include <stdint.h>
//now obviously madness like this isn't used often(who would need invert_bytes for example) a lot of its just for fun and no cpu i've heard of uses such a byte order, but its still included into this header
uint32_t little_to_middle(uint32_t x);
uint32_t reverse_endian32(uint32_t x);
uint32_t invert_bytes(uint32_t x);
uint32_t rev_bendian32(uint32_t x);
uint16_t reverse_endian16(uint16_t x);
uint16_t invert_inner16(uint16_t x);
uint16_t invert_outer16(uint16_t x);
uint16_t flipbyte16(uint16_t x);
uint8_t flipbyte(uint8_t x);
#endif