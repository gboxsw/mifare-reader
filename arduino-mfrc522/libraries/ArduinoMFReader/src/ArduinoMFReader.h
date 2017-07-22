#ifndef ACP_PROJECT_HEADER_H_INCLUDED
#define ACP_PROJECT_HEADER_H_INCLUDED

//----------------------------------------------------------------------
// Includes for component views and required libraries
#include <acp/common/timer/Timer.h>
#include <acp/rfid/mfrc522/MFRC522.h>
#include <acp/messenger/gep_stream_messenger/gepstream_messenger.h>
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Declarations of component views
extern acp_common_timer::TTimer cardCheckTimer;
extern MFRC522 cardReader;
extern acp_messenger_gep_stream::TGEPStreamMessenger<0, 50> messenger;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Eeeprom variables

#define EEPROM_USAGE 0
//----------------------------------------------------------------------


#endif // ACP_PROJECT_HEADER_H_INCLUDED
