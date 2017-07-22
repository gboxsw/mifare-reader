#ifndef MODULES_ACP_COMMON_TIMER_INCLUDE_TIMER_H_
#define MODULES_ACP_COMMON_TIMER_INCLUDE_TIMER_H_

#include <acp/core.h>

namespace acp_common_timer {
	class TTimer;
	class TimerController;

	/********************************************************************************
	 * Controller of timer.
	 ********************************************************************************/
	class TimerController {
		friend class TTimer;
	private:
		// Tick interval
		unsigned long interval;

		// Indicates whether the timer is enabled
		boolean enabled;
	public:
		// Identifier of the looper
		int looperId;

		// Event handler for tick event
		ACPEventHandler tickEvent;

		// Initialization method
		inline void init(unsigned long interval, boolean enabled) {
			this->enabled = enabled;
			this->interval = interval;
			if (enabled) {
				acp::enableLooper(looperId);
			} else {
				acp::disableLooper(looperId);
			}
		}

		// Looper for invoking tick events
		inline unsigned long looper() {
			if (enabled && (tickEvent != NULL)) {
				tickEvent();
			}

			return interval;
		}

		// Enable/disable the timer
		void setEnabled(boolean newEnabled) {
			if (enabled == newEnabled) {
				return;
			}

			enabled = newEnabled;
			if (enabled) {
				acp::enableLooper(looperId);
			} else {
				acp::disableLooper(looperId);
			}
		}
	};

	/********************************************************************************
	 * View for timer controller
	 ********************************************************************************/
	class TTimer
	{
	private:
		// Associated controller
		TimerController &controller;
	public:
		// Constructs timer view associated with given controller
		inline TTimer(TimerController& controller):controller(controller) {
			// Nothing to do
		}

		// Returns whether the timer is enabled
		inline boolean isEnabled() {
			return controller.enabled;
		}

		// Enabled timer
		inline void enable() {
			controller.setEnabled(true);
		}

		// Disable timer
		inline void disable() {
			controller.setEnabled(false);
		}

		// Sets the interval in milliseconds
		inline void setInterval(unsigned long interval) {
			controller.interval = interval;
		}

		// Returns the interval in milliseconds
		inline unsigned long getInterval() {
			return controller.interval;
		}
	};
}

#endif /* MODULES_ACP_COMMON_TIMER_INCLUDE_TIMER_H_ */
