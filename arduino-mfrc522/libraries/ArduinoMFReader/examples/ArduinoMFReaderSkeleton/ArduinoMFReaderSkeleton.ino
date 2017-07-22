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

//----------------------------------------------------------------------
// Event callback for Program.OnStart
void onStart() {
  // TODO Auto-generated callback stub
}

//----------------------------------------------------------------------
// Event callback for cardCheckTimer.OnTick
void onCardCheck() {
  // TODO Auto-generated callback stub
}

//----------------------------------------------------------------------
// Event callback for messenger.OnMessageReceived
void onMessageReceived(const char* message, int messageLength, long messageTag) {
  // TODO Auto-generated callback stub
}
