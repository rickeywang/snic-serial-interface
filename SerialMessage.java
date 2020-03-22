import android.util.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Created by rickey_wang on 22/7/2014.
 */

public class SerialMessage {
    // The character used to indicate the start of a WiFi Message.
    public static final byte StartOfMessage = 0x02;
    // The character used to indicate the end of a WiFi Message.
    public static final byte EndOfMessage = 0x04;
    // The character used to escape any of the UART control characters.
    public static final byte EscapeCharacter = 0x10;
    // Represents the header bit used to control the structure of the WiFi messages.
    public static final byte HeaderBit = (byte) 0x80;
    // Max size of the message (packed message)
    //2047 (payload) + 6 (misc) + 1 (Just because!) = 2053 (final)
    public static final short MSG_MAX_SIZE = 2053;
    // ACK and NAK command IDs
    public static final byte ID_ACK = 0x7F;
    public static final byte ID_NAK = 0x00;

    private static final String TAG = SerialMessage.class.getCanonicalName();

    // ACKing set ups
    public static boolean ACK_lock = false; // User should use this to block transmission until it is true
    private static boolean ACK_timeout = false; // Used to advise that a transmission has timed out
    public static final int ACK_timeout_ms = 100; //Transmission timeout in ms.
    private static int ACK_last_msg_timestamp =0;
    private static Timer ACK_timer = new Timer("ACK Timer", true);
    public static int Timeout_count = 0;

    /**
     * Gets the total length of the payload including escape characters.
     *
     * @param rawStream Any stream lah. Could be a raw message or some payload.
     * @return The integer length of the payload including escape characters.
     */
    public static int getLengthEscapes(byte[] rawStream) {
        int Length = 0;

        for (int i = 0; i < rawStream.length; i++) {
            if (rawStream[i] == StartOfMessage || rawStream[i] == EndOfMessage || rawStream[i] == EscapeCharacter) {
                Length += 2;
            } else {
                Length++;
            }
        }

        return Length;
    }

    /**
     * Returns command ID of a given raw message.
     * !!! IMPORTANT !!!
     * It is ASSUMED that this command will be called by the serial interface for EVERYTHING
     * that it gets, since ACK and NAK processing happens inside the SerialMessage class.
     *
     * @param rawMsg Raw incoming message data
     * @return Command ID
     */

    public static byte getCommand(byte[] rawMsg) throws NAKException {
        byte commandId = (byte) (rawMsg[3] & ~HeaderBit); //Remove the header bit '1'

        // Check if the command is ACK or NAK frame
        if(commandId == ID_ACK && !ACK_timeout){
            // OK so it is an ACK.
            stopACKTimer();
            ACK_lock = false;
        } else if (commandId == ID_NAK && !ACK_timeout){
            // NAK received.. throw the exception!
            stopACKTimer();
            ACK_lock = false;
            throw new NAKException();
        }

        return commandId;
    }

    /**
     * Packages the payload into Brandon's awesome serial UART frame thing that totally kicks ass
     *
     * @param commandId       Payload/Command ID - Identificateur
     * @param acknowledgement ACK bit config. 0 for no ACK req'd, and vice versa.
     * @param payload         Payload/Command itself
     * @return                Fully packaged message goodness ready to be sent out
     */

    public static byte[] packMessage(byte commandId, boolean acknowledgement, byte[] payload) {
        int MessageLength = 0;
        byte Checksum = 0;
        byte Calc = 0;
        int TotalLength = getLengthEscapes(payload);
        List<Byte> RawBytes = new ArrayList<>();

        // Start the message
        RawBytes.add(StartOfMessage);

        // Calculate and append L0: length of payload | 0x8
        Calc = (byte) (TotalLength | HeaderBit);
        RawBytes.add(Calc);
        Checksum += Calc;

        // Calculate and append L1: 0x8 | Ack << 6 | len >> 7
        Calc = (byte) (HeaderBit | ((acknowledgement ? 1 : 0) << 6) | (TotalLength >> 7));
        RawBytes.add(Calc);
        Checksum += Calc;

        // Add the command ID
        Calc = (byte) (HeaderBit | commandId);
        RawBytes.add(Calc);
        Checksum += Calc;

        // Add the payload to the message
        for (byte aPayload : payload) {
            if (aPayload == StartOfMessage || aPayload == EndOfMessage || aPayload == EscapeCharacter) {
                RawBytes.add(EscapeCharacter);
                RawBytes.add((byte) (aPayload | HeaderBit));
                MessageLength += 2;
                Checksum += EscapeCharacter;
                Checksum += (byte) (aPayload | HeaderBit);
            } else {
                RawBytes.add(aPayload);
                MessageLength++;
                Checksum += aPayload;
            }
        }

        // Add the checksum
        RawBytes.add((byte) (HeaderBit | Checksum));
        RawBytes.add(EndOfMessage);

        Log.d(TAG, "Output final - " + bytesToHex(toByteArray(RawBytes)));

        return toByteArray(RawBytes);
    }

    public static byte[] packMessage(byte commandId, boolean acknowledgement, byte payload){
        byte[] payloadArray = new byte[]{payload};

        return packMessage(commandId, acknowledgement, payloadArray);
    }

    /**
     * Convenient for ACK, NAK, and other one-command things.
     * NOTE - the acknowledgement bit should be FALSE for ACK and NAK frames!
     * @param commandId
     * @param acknowledgement
     * @return Fully packaged message goodness ready to be sent out
     */

    public static byte[] packMessage(byte commandId, boolean acknowledgement) {
        // Acknowledgement is always zero for ACK and NAK
        if(commandId == ID_NAK || commandId == ID_ACK){
            acknowledgement=false;
        }

        int MessageLength = 0;
        byte Checksum = 0;
        byte Calc = 0;
        int TotalLength = 0;
        List<Byte> RawBytes = new ArrayList<>();

        // Start the message
        RawBytes.add(StartOfMessage);

        // Calculate and append L0: length of payload | 0x8
        Calc = (byte) (TotalLength | HeaderBit);
        RawBytes.add(Calc);
        Checksum += Calc;

        // Calculate and append L1: 0x8 | Ack << 6 | len >> 7
        Calc = (byte) (HeaderBit | ((acknowledgement ? 1 : 0) << 6) | (TotalLength >> 7));
        RawBytes.add(Calc);
        Checksum += Calc;

        // Add the command ID
        Calc = (byte) (HeaderBit | commandId);
        RawBytes.add(Calc);
        Checksum += Calc;

        // Add the checksum
        RawBytes.add((byte) (HeaderBit | Checksum));
        RawBytes.add(EndOfMessage);

        Log.d(TAG, "Output final - " + bytesToHex(toByteArray(RawBytes)));

        return toByteArray(RawBytes);
    }

    /**
     * Cleans the escape char from raw message, making the 'special' values available.
     *
     * @param message   Message WITH escape char
     * @return          Message WITHOUT escape char
     */

    public static byte[] removeMessageEscapeCharacters(byte[] message) {

        List<Byte> cleanMsg = new ArrayList<>();

        for (int i = 0; i < message.length; i++) {
            if (message[i] != EscapeCharacter) {
                cleanMsg.add(message[i]);
            } else {
                cleanMsg.add((byte) (message[i + 1] & ~HeaderBit));
                i++;
            }
        }

        return toByteArray(cleanMsg);
    }

    /**
     * Unpacks a raw message. Verifies the checksum and ACking requirement.
     * If the message unpacks successfully, the serial interface should
     * send ACK if requested.
     *
     * @param message   Le message
     * @return          Payload from the message
     */
    public static byte[] unpackMessage(byte[] message) throws ChecksumException {
        verifyChecksum(message);

        byte[] StrippedMessage = removeMessageEscapeCharacters(message);

        // There are 6 bytes which aren't part of the payload (see fig 1)
        byte[] ReturnArray = new byte[StrippedMessage.length - 6];

        // Extract the payload
        System.arraycopy(StrippedMessage, 4, ReturnArray, 0, StrippedMessage.length - 2 - 4);

        return ReturnArray;
    }

    /**
     * Verifies the checksum value of a message frame
     * @param rawMsg    The raw message frame
     * @throws          ChecksumException Happens when the checksum mismatches
     */
    public static void verifyChecksum(byte[] rawMsg) throws ChecksumException {
        byte checkSum = 0x0;
        byte recvChkSum = (byte) (rawMsg[rawMsg.length - 2] & (~HeaderBit)); //Remove the HeaderBit

        //Start at ID=1, end at ID=size-2, thus omitting SOM, EOM, and the Checksum itself.
        for (int i = 1; i < rawMsg.length - 2; i++) {
            checkSum += rawMsg[i];
        }

        //Discard the first bit of checkSum, since checksum is only 7 bits.
        checkSum = (byte) (checkSum & (~HeaderBit));

        if(checkSum != recvChkSum){
            throw new ChecksumException(checkSum, recvChkSum);
        }

    }

    public static boolean getACKRequest(byte[] incomingData) {
        int bitLoc = 2;
        return (incomingData[2] & (1 << bitLoc)) != 0;
    }


    public static class ChecksumException extends Exception {
        public byte expectedChksum, actualChksum;
        ChecksumException(byte expected, byte actual) {
            this.expectedChksum = expected;
            this.actualChksum = actual;
        }
    }

    public static class ACKPendingException extends Exception {
    }

    public static class NAKException extends Exception{
    }

    /**
     * Helper Functions
     */
    public static void beginACKTimer(final byte[] lastMessage){
        TimerTask timeout_task = new TimerTask() {
            public void run() {
                ACK_lock = false;
                ACK_timeout = true;
                Timeout_count++;
                DscEvent.emit("serial_ack_timeout", lastMessage);
            }
        };
        ACK_lock = true;
        stopACKTimer(); // Reset before starting again
        ACK_timer = new Timer("ACK Timer", true);
        ACK_timer.schedule(timeout_task, ACK_timeout_ms);
    }


    /**
     * Stops the timeout counter and resets the timeout flag
     */
    private static void stopACKTimer(){
        ACK_timer.cancel();
        ACK_timer.purge();
        ACK_timeout = false;
    }

    public static byte[] toByteArray(List<Byte> in) {
        try {
            final int n = in.size();
            byte ret[] = new byte[n];
            for (int i = 0; i < n; i++) {
                ret[i] = in.get(i);
            }
            return ret;
        } catch (NullPointerException e) {
            Log.w(TAG, "Input is null or contains nulls!");
        }

        return null;
    }

    public static byte[] hexStringToByteArray(String s) {
        int len = s.length();
        byte[] data = new byte[len / 2];
        for (int i = 0; i < len; i += 2) {
            data[i / 2] = (byte) ((Character.digit(s.charAt(i), 16) << 4)
                    + Character.digit(s.charAt(i+1), 16));
        }
        return data;
    }

    final protected static char[] hexArray = "0123456789ABCDEF".toCharArray();

    public static String bytesToHex(byte[] bytes) {
        char[] hexChars = new char[bytes.length * 2];
        for (int j = 0; j < bytes.length; j++) {
            int v = bytes[j] & 0xFF;
            hexChars[j * 2] = hexArray[v >>> 4];
            hexChars[j * 2 + 1] = hexArray[v & 0x0F];
        }
        return new String(hexChars);
    }

    public static String bytesToHex(byte b) {
        byte[] tmp = new byte[1];
        tmp[0] = b;
        return bytesToHex(tmp);
    }

}
