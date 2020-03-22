#pragma once
#define StartOfMessage 0x02
#define EndOfMessage 0x04
#define EscapeCharacter 0x10
#define HeaderBit 0x80
//Max memory bytes req'd for serial protocol:
//2047 (payload) + 6 (misc) + 1 (Just because!) = 2053 (final)
//For ARDUINO ONLY: Put in 100 because SRAM is only 1KB
#define MSG_MAX_SIZE 80

//#include <iostream>
//#include <iomanip>
//#include <fstream>
//#include <cmath>
//#include <iomanip>
//#include <stdio.h>
//#include <cstring>
//#include <cstdio>
//#include <stdlib.h>
//#include <crtdefs.h>

typedef unsigned char BYTE;
typedef unsigned int size_t;

//No 'new' keyword allowed!

class SerialMessage
{
public:
	SerialMessage();
	~SerialMessage();
	int getLengthEscapes(BYTE* rawStream, size_t size);
	int packMessage(BYTE commandId, bool acknowledgement, BYTE* payload, size_t payload_size, BYTE* packedMsg);
	BYTE getCommand(BYTE* rawMsg);
	int unpackMessage(BYTE* rawMsg, BYTE commandId, BYTE* unpackedMsg);
	int removeMessageEscapeCharacters(BYTE* message, int rawMsgLength);
	int verifyChecksum(BYTE* rawMsg, int rawMsgLength);
};

