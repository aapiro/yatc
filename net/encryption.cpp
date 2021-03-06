//////////////////////////////////////////////////////////////////////
// Yet Another Tibia Client
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////

#include "../debugprint.h"
#include "encryption.h"
#include "networkmessage.h"

EncXTEA::EncXTEA()
{
	m_key[0] = 0; m_key[1] = 0; m_key[2] = 0; m_key[3] = 0;
}

bool EncXTEA::encrypt(NetworkMessage& msg)
{
	uint32_t k[4];
	k[0] = m_key[0]; k[1] = m_key[1]; k[2] = m_key[2]; k[3] = m_key[3];

	int32_t messageLength = msg.getSize();

	//add bytes until reach 8 multiple
	uint32_t n;
	if((messageLength % 8) != 0){
		n = 8 - (messageLength % 8);
		msg.addPaddingBytes(n);
		messageLength = messageLength + n;
	}

	int read_pos = 0;
	uint32_t* buffer = (uint32_t*)msg.getBuffer();



	// FIXME (ivucica#2#) implement a recommendation from Pedro Alves from CeGCC development mailing list to fix ARM problems; his text below:
	// --------- BEGIN QUOTE ---------------
	// Why don't you just do something like:

    //   uint8_t* buffer = (uint8_t*)msg.getBuffer();

    //   int read_post = 0;
    //   while (read_pos < messageLength) {
    //           uint32_t v0 = read32(buffer);
    //           uint32_t v1 = read32(buffer + 4);
    //
    //           (...)
    //
    //           write32(buffer, v0);
    //           write32(buffer + 4, v1);
    //           read_pos += 8;
    //   }

	// ... and write read32/write32 as macros or inline functions that
	// just copy and just a few bytes:

	// uin32_t read32(uint8_t*);
	// void write32(uint8_t*, uin32_t);
	// ----------- END QUOTE --------------------

	#ifdef WINCE
	// due to ARM architectural difference, we'll do a memcpy instead of directly accessing
	// casting into something else, and then using it with [] crashes
	//
	buffer = (uint32_t*)malloc(messageLength);
	memcpy(buffer, msg.getBuffer(), messageLength);
	#endif


	while(read_pos < messageLength/4){
		uint32_t v0 = buffer[read_pos], v1 = buffer[read_pos + 1];
		uint32_t delta = 0x61C88647;
		uint32_t sum = 0;

		for(int32_t i = 0; i < 32; i++) {
			v0 += ((v1 << 4 ^ v1 >> 5) + v1) ^ (sum + k[sum & 3]);
			sum -= delta;
			v1 += ((v0 << 4 ^ v0 >> 5) + v0) ^ (sum + k[sum>>11 & 3]);
		}
		buffer[read_pos] = v0; buffer[read_pos + 1] = v1;
		read_pos = read_pos + 2;
	}

	#ifdef WINCE
	// now restoring into original buffer (look above for reasoning)
	memcpy(msg.getBuffer(), buffer, messageLength);
	free(buffer);
	#endif



	msg.addHeader();
	return true;
}

bool EncXTEA::decrypt(NetworkMessage& msg)
{
	if(msg.getReadSize() % 8 != 0){
		DEBUGPRINT(DEBUGPRINT_ERROR, DEBUGPRINT_LEVEL_OBLIGATORY, "[EncXTEA::decrypt]. Not valid encrypted message size %d\n",msg.getReadSize());
		return false;
	}
	//
	uint32_t k[4];
	k[0] = m_key[0]; k[1] = m_key[1]; k[2] = m_key[2]; k[3] = m_key[3];

	uint32_t* buffer = (uint32_t*)msg.getReadBuffer();
	int read_pos = 0;

	int32_t messageLength = msg.getReadSize();

	// FIXME (ivucica#2#) implement a recommendation from Pedro Alves from CeGCC development mailing list to fix ARM problems; his text in another comment  in this file

	#ifdef WINCE
	// due to ARM architectural difference, we'll do a memcpy instead of directly accessing
	// casting into something else, and then using it with [] crashes
	buffer = (uint32_t*)malloc(messageLength);
	memcpy(buffer, msg.getReadBuffer(), messageLength);
	#endif

	while(read_pos < messageLength/4){
		uint32_t v0 = buffer[read_pos], v1 = buffer[read_pos + 1];
		uint32_t delta = 0x61C88647;
		uint32_t sum = 0xC6EF3720;

		for(int32_t i = 0; i < 32; i++) {
			v1 -= ((v0 << 4 ^ v0 >> 5) + v0) ^ (sum + k[sum>>11 & 3]);
			sum += delta;
			v0 -= ((v1 << 4 ^ v1 >> 5) + v1) ^ (sum + k[sum & 3]);
		}
		buffer[read_pos] = v0; buffer[read_pos + 1] = v1;
		read_pos = read_pos + 2;

	}

	#ifdef WINCE
	// now restoring into original buffer (look above for reasoning)
	memcpy(msg.getReadBuffer(), buffer, messageLength);
	free(buffer);
	#endif

	//
	uint16_t newSize;
	if(!msg.getU16(newSize)){
		DEBUGPRINT(DEBUGPRINT_ERROR, DEBUGPRINT_LEVEL_OBLIGATORY, "[EncXTEA::decrypt]. Cant read unencrypted message size\n");
		return false;
	}
	if(newSize > msg.getReadSize()){
		DEBUGPRINT(DEBUGPRINT_ERROR, DEBUGPRINT_LEVEL_OBLIGATORY, "[EncXTEA::decrypt]. Not valid unencrypted message size (new size: %d, old size: %d)\n", newSize, msg.getReadSize());
		return false;
	}

	msg.setReadSize(newSize);
	return true;
}
