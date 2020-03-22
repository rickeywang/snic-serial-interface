#include "SerialMessage.h"
#include <vector>

SerialMessage::SerialMessage()
{
}


SerialMessage::~SerialMessage()
{
}


int SerialMessage::getLengthEscapes(BYTE* rawStream, size_t size)
{
	int Length = 0;

	for (int i = 0; i < size; i++) {
		if (rawStream[i] == StartOfMessage || rawStream[i] == EndOfMessage || rawStream[i] == EscapeCharacter) {
			Length += 2;
		}
		else {
			Length++;
		}
	}

	return Length;
}

int SerialMessage::packMessage(BYTE commandId, bool acknowledgement, BYTE* payload, size_t payload_size, BYTE* packedMsg)
{
	int MessageLength = 0;
	BYTE Checksum = 0;
	BYTE Calc = 0;
	int TotalLength = getLengthEscapes(payload, payload_size);
	std::vector <BYTE> RawBytes;

	// Start the message
	RawBytes.push_back(StartOfMessage);

	// Calculate and append L0: length of payload | 0x8
	Calc = (BYTE)(TotalLength | HeaderBit);
	RawBytes.push_back(Calc);
	Checksum += Calc;

	// Calculate and append L1: 0x8 | Ack << 6 | len >> 7
	Calc = (BYTE)(HeaderBit | (acknowledgement ? 1 : 0) | (TotalLength >> 7));
	RawBytes.push_back(Calc);
	Checksum += Calc;

	// Add the command ID
	Calc = (BYTE)(HeaderBit | commandId);
	RawBytes.push_back(Calc);
	Checksum += Calc;

	// Add the payload to the message
	for (int i = 0; i < payload_size; i++)
	{
		// Special byte processing routine
		if (payload[i] == StartOfMessage || payload[i] == EndOfMessage || payload[i] == EscapeCharacter)
		{
			RawBytes.push_back(EscapeCharacter);
			RawBytes.push_back((BYTE)(payload[i] | HeaderBit));
			MessageLength += 2;
			Checksum += EscapeCharacter;
			Checksum += (BYTE)(payload[i] | HeaderBit);
		}
		else // No special byte. Add in as usual. 
		{
			RawBytes.push_back(payload[i]);
			MessageLength++;
			Checksum += payload[i];
		}
	}

	// Add the checksum
	RawBytes.push_back((BYTE)(Checksum | HeaderBit));
	RawBytes.push_back(EndOfMessage);

	// Copy to the return array
	std::copy(RawBytes.begin(), RawBytes.end(), packedMsg);
	return RawBytes.size();
}

int SerialMessage::removeMessageEscapeCharacters(BYTE* message, int msgLength)
{
	std::vector <BYTE> cleanMsg;

	for (int i = 0; i < msgLength; i++){
		if (message[i] != EscapeCharacter){
			cleanMsg.push_back(message[i]);
		}
		else{
			cleanMsg.push_back((BYTE)(message[i + 1] & ~HeaderBit));
			i++;
		}
	}

	std::copy(cleanMsg.begin(), cleanMsg.end(), message);

	return cleanMsg.size();
}

/*
	Returns 0x00 if the checksum is verified. Else return the computed checksum, so 
	the caller function can compare this expected value against received value lah. 
*/

int SerialMessage::verifyChecksum(BYTE* rawMsg, int rawMsgLength){
	BYTE checkSum = 0x00;
	BYTE recvChkSum = rawMsg[rawMsgLength - 2] & (~HeaderBit); //Remove the HeaderBit

	//char tmp[16];
	//char format[128];
	//sprintf(format, "0x%%.%dX ", 2);
	//sprintf(tmp, format, recvChkSum);
	//std::cout << " recvChkSum " << tmp <<"\n";

	//Start at ID=1, end at ID=size-2, thus omitting SOM, EOM, and the Checksum itself. 
	for (int i = 1; i < rawMsgLength - 2; i++) {
		checkSum += rawMsg[i];

		//char tmp[16];
		//char format[128];
		//sprintf(format, "0x%%.%dX ", 2);
		//sprintf(tmp, format, checkSum);
		//std::cout << tmp;
	}

	//Discard the first bit of checkSum, since checksum is only 7 bits.
	checkSum = (BYTE)(checkSum & (~HeaderBit));

	//sprintf(tmp, format, checkSum);
	//std::cout << "FINAL "<< tmp;

	if (checkSum != recvChkSum){
		return checkSum;
	}
	else {
		return 0;
	}
}

BYTE SerialMessage::getCommand(BYTE* rawMsg){
	return rawMsg[3] & ~HeaderBit;
}

int SerialMessage::unpackMessage(BYTE* rawMsg, BYTE commandId, BYTE* unpackedMsg)
{
	int rawMsgLength = 0;

	// Check message length - SOM to EOM
	for (int i = 0; i < MSG_MAX_SIZE; i++){
		if (rawMsg[i] == EndOfMessage){
			rawMsgLength = i + 1;
			break;
		}
	}

//	std::cout << "Message Size: " << rawMsgLength;

	// Do checksum before removing escape chars first, because removeMessageEscapeCharacters() will mess with the original array. 
	int computedChksum = verifyChecksum(rawMsg, rawMsgLength);
	if (computedChksum > 0) {
		return computedChksum * -1;
	}

	// Return command ID
	getCommand(rawMsg);

	const int newRawMsgLength = removeMessageEscapeCharacters(rawMsg, rawMsgLength);

	// Clean the unpacked array
	memset(unpackedMsg, 0x00, MSG_MAX_SIZE);

	// Extract the payload
	for (int i = 4; i < newRawMsgLength - 2; i++)
	{
		unpackedMsg[i - 4] = rawMsg[i];
	}

	// Return length of the unpacked message, which is 6 bytes less lah
	return newRawMsgLength - 6;
}