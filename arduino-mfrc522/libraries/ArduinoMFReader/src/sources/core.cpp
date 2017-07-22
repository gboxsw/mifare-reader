#include <acp/common/timer/Timer.h>
#include <acp/rfid/mfrc522/MFRC522.h>
#include <acp/messenger/gep_stream_messenger/gepstream_messenger.h>
#include <avr/wdt.h>

#ifdef __cplusplus
extern "C" {
void setup();
void loop();
}
#endif

//----------------------------------------------------------------------
// User defined event handlers
extern void onStart();
extern void onMessageReceived(const char*, int, long);
extern void onCardCheck();
// End of user defined event handlers
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Non-public area
namespace acp_private {
  // Controller for cardCheckTimer
  acp_common_timer::TimerController controller_0;
  // Controller for messenger
  acp_messenger_gep_stream::GEPStreamController<0, 50> controller_1;
}
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Component views (public objects)
acp_common_timer::TTimer cardCheckTimer(acp_private::controller_0);
MFRC522 cardReader(10, 9);
acp_messenger_gep_stream::TGEPStreamMessenger<0, 50> messenger(acp_private::controller_1);
// End of component views (public objects)
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// EEPROM items (public objects)

// End of EEPROM items (public objects)
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Initialization of EEPROM data
namespace acp_private {
void initializeEeprom() {

}
}
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Method wrappers
namespace acp_private {

}
//----------------------------------------------------------------------

// Loopers
namespace acp_private {

// Type for pointer to LooperHandler function
typedef unsigned long (*LooperHandler)();

// Looper record
struct Looper {
	// Time of the next handler call
	unsigned long nextCall;
	// State of the looper
	byte state;
	// Handler of looper
	LooperHandler handler;
};

// Generated looper handlers
unsigned long looper_handler_0() {
  return acp_private::controller_0.looper();
}
// End of looper handlers

#define ENABLED 1
#define DISABLED 0
#define EXECUTED_ENABLED 2
#define EXECUTED_DISABLED 3

// Loopers
#define LOOPERS_COUNT 1
Looper loopers[LOOPERS_COUNT] = {   {0, ENABLED, looper_handler_0} };
Looper* pq[LOOPERS_COUNT] = {loopers + 0};
int pqSize = LOOPERS_COUNT;
unsigned long now = 0;

// Process loopers
inline void processLoopers() {
	if (pqSize == 0) {
		return;
	}

	// Update current time from the view of looper
	now = millis();

	// Process expired handlers
	while (true) {
		Looper* activeLooper = pq[0];

		// Check the first expected looper
		if ((pqSize == 0) || (activeLooper->nextCall > now)) {
			break;
		}

		// Execute handler and store time of the next call
		activeLooper->state = EXECUTED_ENABLED;
		activeLooper->nextCall = now + activeLooper->handler();

		// Move readPos to position of active looper
		Looper** readPos = pq;
		while (*readPos != activeLooper) {
			readPos++;
		}

		// Set writePos to position of active looper and readPos to position of next looper
		Looper** writePos = readPos;
		readPos++;
		Looper** const end = pq + pqSize;

		if (activeLooper->state == EXECUTED_ENABLED) {
			// EXECUTED_ENABLED
			const unsigned long nextCall = activeLooper->nextCall;
			while ((readPos != end) && ((*readPos)->nextCall <= nextCall)) {
				*writePos = *readPos;
				writePos++;
				readPos++;
			}
			*writePos = activeLooper;
			activeLooper->state = ENABLED;
		} else {
			// EXECUTED_DISABLED
			while (readPos != end) {
				*writePos = *readPos;
				writePos++;
				readPos++;
			}
			pqSize--;
			activeLooper->state = DISABLED;
		}
	}
}
}

// Accessible controller methods
namespace acp {

using namespace acp_private;

// Enables a looper
void enableLooper(int looperId) {
	Looper* const looper = &loopers[looperId];
	if ((looper->state == ENABLED) || (looper->state == EXECUTED_ENABLED)) {
		return;
	}

	if (looper->state == EXECUTED_DISABLED) {
		looper->state = EXECUTED_ENABLED;
		return;
	}

	looper->state = ENABLED;
	looper->nextCall = now;

	Looper** writePos = pq + pqSize;
	Looper** readPos = writePos - 1;
	while (writePos != pq) {
		*writePos = *readPos;
		writePos--;
		readPos--;
	}

	pqSize++;
	pq[0] = looper;
}

// Disables a looper
void disableLooper(int looperId) {
	Looper* const looper = &loopers[looperId];
	if ((looper->state == DISABLED) || (looper->state == EXECUTED_DISABLED)) {
		return;
	}

	if (looper->state == EXECUTED_ENABLED) {
		looper->state = EXECUTED_DISABLED;
		return;
	}

	looper->state = DISABLED;

	Looper** readPos = pq;
	while (*readPos != looper) {
		readPos++;
	}

	Looper** const end = pq + pqSize;
	Looper** writePos = readPos;
	readPos++;
	while (readPos != end) {
		*writePos = *readPos;
		writePos++;
		readPos++;
	}
	pqSize--;
}
}


// Autogenerated setup
void setup() {
  wdt_disable();
  // Controller for cardCheckTimer
  acp_private::controller_0.looperId = 0;
  acp_private::controller_0.tickEvent = onCardCheck;
  acp_private::controller_0.init(250ul, true);
  // Controller for messenger
  acp_private::controller_1.messageReceivedEvent = onMessageReceived;
  // Call of the OnStart event
  onStart();
  wdt_enable(9);
}

// Autogenerated loop
void loop() {
  wdt_reset();
  acp_private::controller_1.loop();
  // Process loopers
  acp_private::processLoopers();
}
