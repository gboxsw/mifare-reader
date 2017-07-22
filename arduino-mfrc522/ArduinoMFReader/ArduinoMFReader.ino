//----------------------------------------------------------------------
// Includes required to build the sketch (including ext. dependencies)
#include <ArduinoMFReader.h>
#include <SPI.h>
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Summary of available objects:
// cardCheckTimer (acp.common.timer)
// cardReader (acp.rfid.mfrc522)
// messenger (acp.messenger.gep_stream_messenger)
//----------------------------------------------------------------------

// Maximal length of response
#define MAX_RESPONSE_LENGTH 30

// Identification of the other GEP endpoint (0 - point-to-point connection is assumed).
#define ENDPOINT_ID 0

// Command codes
enum CommandCode: byte {
  RESET = 1,
  SET_KEY = 2,
  READ_BLOCK = 3,
  WRITE_BLOCK = 4,
  READ_SECTOR_TRAILER = 5,
  WRITE_SECTOR_TRAILER = 6
};

// Codes of messages sent by the reader
enum ReaderMsgCode: byte {
  COMMAND_OK = 1,
  COMMAND_FAILED = 2,
  CARD_DETECTED = 3,
  CARD_REMOVED = 4
};

// Codes of messages sent by the reader
enum CardType: byte {
    UNKNOWN = 0,
    PICC_TYPE_ISO_14443_4 = 1,
    PICC_TYPE_ISO_18092 = 2,
    PICC_TYPE_MIFARE_MINI = 3,
    PICC_TYPE_MIFARE_1K = 4,
    PICC_TYPE_MIFARE_4K = 5,
    PICC_TYPE_MIFARE_UL = 6,
    PICC_TYPE_MIFARE_PLUS = 7,
    PICC_TYPE_TNP3XXX = 8
};

// Type of key
enum KeyType: byte {
  NONE = 0,
  KEY_A = 1,
  KEY_B = 2
};

// Indicates whether a card is activated
boolean activeCard = false;

// Type of active card
CardType cardType = CardType::UNKNOWN;

// Number of available blocks on active card
byte blockCount = 0;

// Indicates whether reset of card is required
boolean resetRequired = false;

// Indicates that communication with card failed
boolean cardFailed = false;

// Number of checks in row when presence of card was not detected. 
int cardPresentFails = 0;

// Key for accessing data 
MFRC522::MIFARE_Key key;

// Type of active key
KeyType keyType = KeyType::NONE;

// Authenticated sector
int authenticatedSector = -1;

//----------------------------------------------------------------------
// Event callback for Program.OnStart
void onStart() {
  Serial.begin(9600);
  messenger.setStream(Serial);
  
  SPI.begin();			
  cardReader.PCD_Init();
}

//----------------------------------------------------------------------
// Stops the active card
void stopCard() {
  if (!activeCard) {
    return; 
  }
  
  cardReader.PICC_HaltA();
  cardReader.PCD_StopCrypto1();  
  resetRequired = false;  
  activeCard = false;
  keyType = KeyType::NONE;
  authenticatedSector = -1;
  cardFailed = false;
}


//----------------------------------------------------------------------
// Send response with notification that command failed.
void sendSimpleCommandResponse(long messageTag, bool success) {
   char response[1];
   response[0] = success ? ReaderMsgCode::COMMAND_OK : ReaderMsgCode::COMMAND_FAILED;
   messenger.sendMessage(ENDPOINT_ID, response, 1, messageTag); 
}

//----------------------------------------------------------------------
// Handle command that sets active key.
void handleSetKeyCommand(const byte* message, int messageLength, long messageTag) {
  // validate message
  if (messageLength != MFRC522::MIFARE_Misc::MF_KEY_SIZE+1) {
    sendSimpleCommandResponse(messageTag, false);
    return;    
  }

  // determine key type
  if (message[0] == 1) {
    keyType = KeyType::KEY_A; 
  } else if (message[0] == 2) {
    keyType = KeyType::KEY_B; 
  } else {
    sendSimpleCommandResponse(messageTag, false);
    return;       
  }

  message++;
  messageLength--;
  // copy key
  for (byte i = 0; i < MFRC522::MIFARE_Misc::MF_KEY_SIZE; i++) {
    key.keyByte[i] = message[i];
  }

  // clear authentication
  authenticatedSector = -1;

  // confirm execution of command
  sendSimpleCommandResponse(messageTag, true);
}

//----------------------------------------------------------------------
// Returns trailer block of a sector
int getTrailerBlockOfSector(byte sectorId) {
  if (sectorId < 32) {
    return sectorId * 4 + 3;  
  } 

  if (sectorId < 40) {
    return 128 + (sectorId - 32) * 16 + 15;
  }

  return -1;
}

//----------------------------------------------------------------------
// Returns sector to which the block belongs.
byte getSectorOfBlock(byte blockId) {
  if (blockId < 128) {
    return blockId / 4;   
  } else {
    return 32 + (blockId - 128) / 16;
  } 
}

//----------------------------------------------------------------------
// Prepares block for reading/writing and returns whether operation completed successfully
bool prepareCardBlock(int blockId) {
  // validate state of reader
  if (!activeCard || (keyType == KeyType::NONE) || resetRequired) {
    return false;       
  }  

  // validate block id
  if ((blockId < 0) || (blockId >= blockCount)) {
    return false;
  } 

  // determine sector id
  byte sectorId = getSectorOfBlock(blockId); 
  
  // check authetication state
  if (authenticatedSector == sectorId) {
    return true;
  }

  // determine trailer block for authentication
  int trailerBlockId = getTrailerBlockOfSector(sectorId);
  if (trailerBlockId < 0) {
    return false;
  }

  // clear authentication
  authenticatedSector = -1;

  // authenticate
  MFRC522::StatusCode status = (MFRC522::StatusCode) cardReader.PCD_Authenticate((keyType == KeyType::KEY_A) ? MFRC522::PICC_CMD_MF_AUTH_KEY_A : MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlockId, &key, &(cardReader.uid));
  if (status != MFRC522::STATUS_OK) {
    cardFailed = true;
    return false;
  }

  authenticatedSector = sectorId;
  return true;
}

//----------------------------------------------------------------------
// Handle command that reads a block
void handleReadBlockCommand(const byte* message, int messageLength, long messageTag) {
  // validate message
  if (messageLength != 1) {
    sendSimpleCommandResponse(messageTag, false);
    return;    
  }

  // determine block id and prepare card for reading/writing the block
  int blockId = *message;
  if (!prepareCardBlock(blockId)) {
    sendSimpleCommandResponse(messageTag, false);      
    return;
  }

  // 1 byte for status, 16 bytes block data, 2 bytes overhead for CRC checksum
  byte response[1 + 18];
  response[0] = ReaderMsgCode::COMMAND_OK;
  byte byteCount = sizeof(response) - 1;
  MFRC522::StatusCode status = cardReader.MIFARE_Read(blockId, &response[1], &byteCount);
  if (status != MFRC522::STATUS_OK) {
    sendSimpleCommandResponse(messageTag, false);  
    cardFailed = true;
    return;
  }
  
  messenger.sendMessage(ENDPOINT_ID, response, 1 + byteCount - 2, messageTag); 
}

//----------------------------------------------------------------------
// Handle command that writes a block
void handleWriteBlockCommand(const byte* message, int messageLength, long messageTag) {
  // validate message
  if ((messageLength < 1) || (messageLength > 1 + 16)) {
    sendSimpleCommandResponse(messageTag, false);
    return;    
  }

  // determine block id and prepare card for reading/writing the block
  int blockId = *message; 
  if (!prepareCardBlock(blockId)) {
    sendSimpleCommandResponse(messageTag, false);      
    return;
  }

  // trailer block cannot be written using this command
  if (blockId == getTrailerBlockOfSector(getSectorOfBlock(blockId))) {
    sendSimpleCommandResponse(messageTag, false);      
    return;
  }

  message++;
  messageLength--;

  // write block data
  MFRC522::StatusCode status = (MFRC522::StatusCode) cardReader.MIFARE_Write(blockId, message, messageLength);
  if (status != MFRC522::STATUS_OK) {
    sendSimpleCommandResponse(messageTag, false);   
    cardFailed = true;
    return;  
  }
  
  // validate write
  byte buffer[18];
  byte byteCount = sizeof(buffer);
  status = cardReader.MIFARE_Read(blockId, buffer, &byteCount);
  if (status != MFRC522::STATUS_OK) {
    sendSimpleCommandResponse(messageTag, false);  
    cardFailed = true;
    return;
  }

  bool allMatch = true;
  if (messageLength != byteCount - 2) {
    allMatch = false;
  } else {
    for (byte i=0; i<messageLength; i++) {
      if (buffer[i] != message[i]) {
        allMatch = false;
        break;
      }
    }
  } 
  
  sendSimpleCommandResponse(messageTag, allMatch);  
}

//----------------------------------------------------------------------
// Handle command that reads sector trailer.
void handleReadSectorTrailerCommand(const byte* message, int messageLength, long messageTag) {
  // validate message
  if (messageLength != 1) {
    sendSimpleCommandResponse(messageTag, false);
    return;    
  }

  // determine trailer block of sector and prepare card for reading/writing this block
  int trailerBlockId = getTrailerBlockOfSector(*message);
  if (!prepareCardBlock(trailerBlockId)) {
    sendSimpleCommandResponse(messageTag, false);      
    return;
  }

  // 16 bytes block data, 2 bytes overhead for CRC checksum
  byte trailerReadBuffer[18];
  byte byteCount = sizeof(trailerReadBuffer);
  MFRC522::StatusCode status = cardReader.MIFARE_Read(trailerBlockId, trailerReadBuffer, &byteCount);
  if (status != MFRC522::STATUS_OK) {
    sendSimpleCommandResponse(messageTag, false);  
    cardFailed = true;
    return;
  }

  // prepare response as 1B COMMAND STATUS, 4B ACCESS-BITS, 6B KEY A, 6B KEY B, 1B GPB (total 18B)
  byte response[1+4+6+6+1];
  response[0] = ReaderMsgCode::COMMAND_OK;
  byte c1  = trailerReadBuffer[7] >> 4;
  byte c2  = trailerReadBuffer[8] & 0xF;
  byte c3  = trailerReadBuffer[8] >> 4;
  byte c1_ = trailerReadBuffer[6] & 0xF;
  byte c2_ = trailerReadBuffer[6] >> 4;
  byte c3_ = trailerReadBuffer[7] & 0xF;
  bool invertedError = (c1 != (~c1_ & 0xF)) || (c2 != (~c2_ & 0xF)) || (c3 != (~c3_ & 0xF));
  response[1] = ((c1 & 1) << 2) | ((c2 & 1) << 1) | ((c3 & 1) << 0);
  response[2] = ((c1 & 2) << 1) | ((c2 & 2) << 0) | ((c3 & 2) >> 1);
  response[3] = ((c1 & 4) << 0) | ((c2 & 4) >> 1) | ((c3 & 4) >> 2);
  response[4] = ((c1 & 8) >> 1) | ((c2 & 8) >> 2) | ((c3 & 8) >> 3);

  if (invertedError) {
    sendSimpleCommandResponse(messageTag, false);  
    return;  
  }

  // copy KEY A
  for (byte i=0; i<6; i++) {
    response[5+i] = trailerReadBuffer[i]; 
  }

  // copy KEY B
  for (byte i=0; i<6; i++) {
    response[5+6+i] = trailerReadBuffer[10+i]; 
  }

  // copy General Purpose Byte
  response[17] = trailerReadBuffer[9];

  // send response
  messenger.sendMessage(ENDPOINT_ID, response, sizeof(response), messageTag); 
}

//----------------------------------------------------------------------
// Handle command that writes a sector trailer.
void handleWriteSectorTrailerCommand(const byte* message, int messageLength, long messageTag) {
  // validate message 1B sector 4B access bits 6B KeyA 6B KeyB 1B GPB
  if (messageLength != 18) {
    sendSimpleCommandResponse(messageTag, false);
    return;    
  }

  // determine trailer block of sector and prepare card for reading/writing this block
  int trailerBlockId = getTrailerBlockOfSector(*message);
  if (!prepareCardBlock(trailerBlockId)) {
    sendSimpleCommandResponse(messageTag, false);      
    return;
  }

  message++;
  messageLength--;

  // prepare bufferes
  byte trailerWriteBuffer[16];
  byte trailerReadBuffer[18];
  
  // set access bits
  cardReader.MIFARE_SetAccessBits(&trailerWriteBuffer[6], message[0], message[1], message[2], message[3]);
  message += 4;
  
  // copy Key A
  for (byte i=0; i<6; i++) {
    trailerWriteBuffer[i] = message[i];
  }
  message += 6;
  
  // copy KeyB
  for (byte i=0; i<6; i++) {
    trailerWriteBuffer[10+i] = message[i];
  }
  message += 6;

  // copy GPB
  trailerWriteBuffer[9] = message[0];
  
  // read trailer block
  byte byteCount = sizeof(trailerReadBuffer);
  MFRC522::StatusCode status = cardReader.MIFARE_Read(trailerBlockId, trailerReadBuffer, &byteCount);
  if (status != MFRC522::STATUS_OK) {
    sendSimpleCommandResponse(messageTag, false);  
    cardFailed = true;;
    return;
  }

  // compare content of trailer block with desired content
  bool allMatch = true;
  for (byte i=0; i<sizeof(trailerWriteBuffer); i++) {
    if (trailerReadBuffer[i] != trailerWriteBuffer[i]) {
      allMatch = false;
      break;    
    }
  }

  if (allMatch) {
    sendSimpleCommandResponse(messageTag, true);  
    return;  
  }

  // write trailer block
  byteCount = sizeof(trailerWriteBuffer);
  status = (MFRC522::StatusCode) cardReader.MIFARE_Write(trailerBlockId, trailerWriteBuffer, byteCount);
  if (status != MFRC522::STATUS_OK) {
    sendSimpleCommandResponse(messageTag, false);   
    cardFailed = true;
    return;  
  }

  sendSimpleCommandResponse(messageTag, true);  
}

//----------------------------------------------------------------------
// Event callback for messenger.OnMessageReceived
void onMessageReceived(const char* message, int messageLength, long messageTag) {
  // ignore malformed messages, i.e. messages without data or without tag.
  if ((messageLength == 0) || (messageTag < 0)) {
    return;
  }

  // retrieve command code
  CommandCode commandCode = message[0];
  messageLength--;
  message++;

  // dispatch commands
  cardFailed = false;
  if (commandCode == CommandCode::SET_KEY) {
    handleSetKeyCommand((byte*)message, messageLength, messageTag);
  } else if (commandCode == CommandCode::READ_BLOCK) {
    handleReadBlockCommand((byte*)message, messageLength, messageTag);
  } else if (commandCode == CommandCode::WRITE_BLOCK) {
    handleWriteBlockCommand((byte*)message, messageLength, messageTag);
  } else if (commandCode == CommandCode::READ_SECTOR_TRAILER) {
    handleReadSectorTrailerCommand((byte*)message, messageLength, messageTag);
  } else if (commandCode == CommandCode::WRITE_SECTOR_TRAILER) {
    handleWriteSectorTrailerCommand((byte*)message, messageLength, messageTag);
  } else if (commandCode == CommandCode::RESET) {
    stopCard();
    sendSimpleCommandResponse(messageTag, true);      
    onCardCheck();
  } else {
    // unknown command
    sendSimpleCommandResponse(messageTag, false);
  }
}

//----------------------------------------------------------------------
// Realizes setup for card of given type
void setupCard(MFRC522::PICC_Type piccType) {
  blockCount = 0;
  cardType = CardType::UNKNOWN;
    
  switch (piccType) {
    case MFRC522::PICC_Type::PICC_TYPE_ISO_14443_4: 
      cardType = CardType::PICC_TYPE_ISO_14443_4;
      return;
    case MFRC522::PICC_Type::PICC_TYPE_ISO_18092: 
      cardType = CardType::PICC_TYPE_ISO_18092;
      return;
    case MFRC522::PICC_Type::PICC_TYPE_MIFARE_MINI: 
      cardType = CardType::PICC_TYPE_MIFARE_MINI;      
      blockCount = 20;    
      return;
    case MFRC522::PICC_Type::PICC_TYPE_MIFARE_1K: 
      cardType = CardType::PICC_TYPE_MIFARE_1K;  
      blockCount = 64;    
      return;
    case MFRC522::PICC_Type::PICC_TYPE_MIFARE_4K: 
      cardType = CardType::PICC_TYPE_MIFARE_4K;      
      blockCount = 255;
      return;
    case MFRC522::PICC_Type::PICC_TYPE_MIFARE_UL: 
      cardType = CardType::PICC_TYPE_MIFARE_UL;      
      return;
    case MFRC522::PICC_Type::PICC_TYPE_MIFARE_PLUS: 
      cardType = CardType::PICC_TYPE_MIFARE_PLUS;
      return;
    case MFRC522::PICC_Type::PICC_TYPE_TNP3XXX: 
      cardType = CardType::PICC_TYPE_TNP3XXX;                                                                  
      return;
  }
}

//----------------------------------------------------------------------
// Event callback for cardCheckTimer.OnTick
void onCardCheck() {
  if (resetRequired) {
    stopCard();
   
    // notify client that card has been removed
    char response[1];
    response[0] = ReaderMsgCode::CARD_REMOVED;
    messenger.sendMessage(ENDPOINT_ID, response, 1, 0); 
  }

  if (activeCard) {
    return;
  }
  
  if (!cardReader.PICC_IsNewCardPresent()) {
    return;
  }

  if (!cardReader.PICC_ReadCardSerial()) {
    return;  
  }

  // initialize and setup new active card
  activeCard = true;
  cardPresentFails = 0;
  setupCard(cardReader.PICC_GetType(cardReader.uid.sak));
  byte uidLen = cardReader.uid.size;
  if (uidLen > 10) {
    uidLen = 10;
  }
  
  // notify client that a card is detected with a message:
  // [CODE 1B][CARD TYPE 1B][NUMBER OF BLOCKS 1B][UUID 4-10B]
  char response[3+10];
  response[0] = ReaderMsgCode::CARD_DETECTED;
  response[1] = cardType;
  response[2] = blockCount;
  memcpy(&response[3], cardReader.uid.uidByte, uidLen);
  messenger.sendMessage(ENDPOINT_ID, response, 3+uidLen, 0);  
}

