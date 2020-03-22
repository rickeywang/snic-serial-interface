/*
Software serial multple serial test

Receives from the hardware serial, sends to software serial.
Receives from software serial, sends to hardware serial.

The circuit:
* RX is digital pin 10 (connect to TX of other device)
* TX is digital pin 11 (connect to RX of other device)

Note:
Not all pins on the Mega and Mega 2560 support change interrupts,
so only the following can be used for RX:
10, 11, 12, 13, 50, 51, 52, 53, 62, 63, 64, 65, 66, 67, 68, 69

Not all pins on the Leonardo support change interrupts,
so only the following can be used for RX:
8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI).

created back in the mists of time
modified 25 May 2012
by Tom Igoe
based on Mikal Hart's example

This example code is in the public domain.

*/
#include <StandardCplusplus.h>
#include <stdio.h>
#include <SerialMessage.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(11, 12); // RX, TX
SerialMessage sMsg;
char charsCollected[MSG_MAX_SIZE*2] = "", serial_in, oneHexValue[2] = "0";
BYTE packedMsg[MSG_MAX_SIZE], numCharCollected, status, mode, command[MSG_MAX_SIZE], incomingMsg[MSG_MAX_SIZE], unpackedMsg[MSG_MAX_SIZE];
int packedMsgSize;

int byteCountNow = 0;
int byteCountUnpacked = 0;
BYTE incomingDataByte, commandByte;
//Note: these two are mutually exclusive (cannot occur at once!)
bool EOMReceived = false;
bool SOMReceived = false;

void clcCmd() {
	Serial.flush();
	charsCollected[0] = '\0'; //Clears command for next round
}

void setup()
{
	// Open serial communications and wait for port to open:
	Serial.begin(9600);
	while (!Serial) {
		; // wait for serial port to connect. Needed for Leonardo only
	}

	Serial.println("Goodnight moon!");

	// set the data rate for the SoftwareSerial port
	mySerial.begin(9600);
	mySerial.println("Hello, world?");
}

void loop() // run over and over
{
	if (mySerial.available()){
		//char tmp[16];
		//char format[128];
		//sprintf(format, "0x%%.%dX", 2);
		//sprintf(tmp, format, mySerial.read());
		//Serial.println(tmp);

		incomingDataByte = mySerial.read();

		// First check if SOM or EOM has been received...
		if (incomingDataByte == StartOfMessage) {
			byteCountNow = 0; //Reset byte received count
			SOMReceived = true;
			EOMReceived = false;
		}
		else if (incomingDataByte == EndOfMessage) {
			EOMReceived = true;
			SOMReceived = false;
		}

		if (SOMReceived) {
			//Add to stack, including the SOM
			incomingMsg[byteCountNow] = incomingDataByte;
		}
		else if (EOMReceived) {
			//Reset the switch
			EOMReceived = false;
			//Add the EOM into our stack
			incomingMsg[byteCountNow] = incomingDataByte;
			//Alert appropriate parties
			Serial.print("\nReceived data: 0x");
			for (int i = 0; i <= byteCountNow; i++){
				char tmp[16];
				char format[128];
				sprintf(format, "%%.%dX ", 2);
				sprintf(tmp, format, incomingMsg[i]);
				Serial.print(tmp);
			}
				
			byteCountUnpacked = sMsg.unpackMessage(incomingMsg, commandByte, unpackedMsg);

			if (byteCountUnpacked > 0){
				Serial.print("\n Unpacked: Command 0x");
				char tmp[16];
				char format[128];
				sprintf(format, "%%.%dX ", 2);
				sprintf(tmp, format, commandByte);
				Serial.println(tmp);
				Serial.print(" Payload 0x");
				for (int i = 0; i < byteCountUnpacked; i++){
					sprintf(tmp, format, unpackedMsg[i]);
					Serial.print(tmp);
				}
			}
			else {
				Serial.print("\nChecksum mismatch. Computed checksum byteCountUnpacked = DEC");
				Serial.println(byteCountUnpacked);
			}
			
			
			//Clean up the array for next round.
			memset(incomingMsg, 0x00, MSG_MAX_SIZE);
			memset(unpackedMsg, 0x00, MSG_MAX_SIZE);
		}

		byteCountNow++;
	}
		
	//Wait until a character comes in on the Serial port.
	if (Serial.available()){
		//Decide what to do based on the character received.
		serial_in = toupper(Serial.read());

		switch (serial_in){
		case 'C': //Send Message
			//Collect Commands
			clcCmd();
			Serial.flush();
			Serial.println(F("\nEnter Message: "));
			while (serial_in != 'W') {
				serial_in = toupper(Serial.read());
				if ((serial_in >= '0' && serial_in <= '9') ||
					(serial_in >= 'A' && serial_in <= 'F'))
					if (strlen(charsCollected) < MSG_MAX_SIZE * 2) {
						strncat(charsCollected, &serial_in, 1);
						//Echo the command string as it is composed
						Serial.print(serial_in);
					}
					else Serial.println(F("Too many hex digits!"));
					//else Serial.println(F("Invalid command!"));
						Serial.flush();
			}
			//Send Commands
			numCharCollected = strlen(charsCollected);
			//Silently ignore empty lines, this also gives us CR & LF support
			if (numCharCollected) {
				if (numCharCollected % 2 == 0){
					memset(command, 0x00, MSG_MAX_SIZE); //Reset first

					//Convert HEX char* to eqv byte values
					for (int i = 0; i < (numCharCollected / 2); i++) {
						strncpy(&oneHexValue[0], &charsCollected[i * 2], 2);
						command[i] = strtoul(oneHexValue, NULL, 16);
					}

					//Pack and send it out
					packedMsgSize = sMsg.packMessage(0x01, false, (BYTE*)command, (size_t)(numCharCollected / 2), packedMsg);

					char tmp[16];
					char format[128];
					Serial.print("\nSending 0x");
					sprintf(format, "%%.%dX ", 2);

					for (int i = 0; i < packedMsgSize; i++){
						//Serial.print(, HEX);
						sprintf(tmp, format, packedMsg[i]);
						Serial.print(tmp);
						mySerial.write(packedMsg[i]);
					}
				}
				else Serial.println(F("Odd number of hex digits, need even!"));
			}
			
			//mySerial.write(packedMsg, packedMsgSize);
			clcCmd();
			break;
		case '?':
			Serial.println(F("Autobike Serial Interface Testing Console"));
			Serial.println(F("* 0-z    - arguments"));;
			Serial.println(F("* c      - Compose command, 0 to F, at most 200 hex digits (100 bytes)"));
			Serial.println(F("           are accepted due to Arduino memory limitations."));
			Serial.println(F("* w      - Write (send) command"));
			Serial.println(F("* ?      - display this list"));
			Serial.flush();
			break;
			//If we get any other character
			//copy it to the command string.
		default:
			/*Serial.print("serial_in=");
			Serial.print(serial_in);
			Serial.print("\n");
			*/
			strncat(charsCollected, &serial_in, 1);
			//Echo the command string as it is composed
			Serial.print(serial_in);
			Serial.flush();
			break;
		}
	}
}
