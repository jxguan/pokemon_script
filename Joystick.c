/*
Nintendo Switch Fightstick - Proof-of-Concept

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

/** \file
 *
 *  Main source file for the posts printer demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "Joystick.h"

extern const uint8_t image_data[0x12c1] PROGMEM;

// Main entry point.
int main(void) {
	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	// Once that's done, we'll enter an infinite loop.
	for (;;)
	{
		// We need to run our task to process and deliver data for our IN and OUT endpoints.
		HID_Task();
		// We also need to run the main USB management task.
		USB_USBTask();
	}
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
	// We need to disable watchdog if enabled by bootloader/fuses.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// We need to disable clock division before initializing the USB hardware.
	clock_prescale_set(clock_div_1);
	// We can then initialize our hardware and peripherals, including the USB stack.

	#ifdef ALERT_WHEN_DONE
	// Both PORTD and PORTB will be used for the optional LED flashing and buzzer.
	#warning LED and Buzzer functionality enabled. All pins on both PORTB and \
PORTD will toggle when printing is done.
	DDRD  = 0xFF; //Teensy uses PORTD
	PORTD =  0x0;
                  //We'll just flash all pins on both ports since the UNO R3
	DDRB  = 0xFF; //uses PORTB. Micro can use either or, but both give us 2 LEDs
	PORTB =  0x0; //The ATmega328P on the UNO will be resetting, so unplug it?
	#endif
	// The USB stack should be initialized last.
	USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
	// We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
	// We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	// We setup the HID report endpoints.
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

	// We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
	// We can handle two control requests: a GetReport and a SetReport.

	// Not used here, it looks like we don't receive control request from the Switch.
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
	// If the device isn't connected and properly configured, we can't do anything here.
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;

	// We'll start with the OUT endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
	// We'll check to see if we received something on the OUT endpoint.
	if (Endpoint_IsOUTReceived())
	{
		// If we did, and the packet has data, we'll react to it.
		if (Endpoint_IsReadWriteAllowed())
		{
			// We'll create a place to store our data received from the host.
			USB_JoystickReport_Output_t JoystickOutputData;
			// We'll then take in that data, setting it up in our storage.
			while(Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL) != ENDPOINT_RWSTREAM_NoError);
			// At this point, we can react to this data.

			// However, since we're not doing anything with this data, we abandon it.
		}
		// Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
		Endpoint_ClearOUT();
	}

	// We'll then move on to the IN endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
	// We first check to see if the host is ready to accept data.
	if (Endpoint_IsINReady())
	{
		// We'll create an empty report.
		USB_JoystickReport_Input_t JoystickInputData;
		// We'll then populate this report with what we want to send to the host.
		GetNextReport(&JoystickInputData);
		// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
		while(Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL) != ENDPOINT_RWSTREAM_NoError);
		// We then send an IN packet on this endpoint.
		Endpoint_ClearIN();
	}
}

typedef enum {
	SYNC_CONTROLLER,
	SYNC_POSITION,
	STOP_X,
	STOP_Y,
	MOVE_X,
	MOVE_Y,
	DONE
} State_t;
State_t state = SYNC_CONTROLLER;

#define ECHOES 2
#define BUTTON_DURATION 10
int echoes = 0;
USB_JoystickReport_Input_t last_report;

int xpos = 0;
int ypos = 0;
int portsval = 0;

// Sync the controller. MUST HAVE!
Step_t SyncController[8] = {
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_L | SWITCH_R, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_L | SWITCH_R, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION}
};

// Recalls to the front of the house.
Step_t Recall[11] = {
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_X, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  // Wait for map to pop
  {0, STICK_CENTER, STICK_CENTER, 300},
  {0, 170, STICK_CENTER, 25},
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  // Wait for the recall process to complete
  {0, STICK_CENTER, STICK_CENTER, 300}
};

Step_t BikeBig[2] = {
  {0, STICK_MAX, STICK_CENTER, 75},
  {SWITCH_B, STICK_MAX, STICK_CENTER, BUTTON_DURATION}
};

Step_t Bike[1] = {
  {0, STICK_MAX, STICK_CENTER, 100},
};

Step_t BreakEgg[2] = {
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_B, STICK_CENTER, STICK_CENTER, BUTTON_DURATION}
};

// Starts from the front of the house, on a bike. Gets an egg
// from the lady (or not). Ends up on a bike. Notice the sequence is A-A-B-A-B. 
// This is designed specifically so that if there is no egg available, the
// player will properly end the conversation with the lady and walk away from
// her. DO NOT change this unless you really understand the reasoning.
Step_t GetEgg[22] = {
  {0, STICK_CENTER, STICK_CENTER, 300},
  {SWITCH_PLUS, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_MIN, STICK_MIN, 300},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  // Music plays for "new egg". This is a long wait.
  {0, STICK_CENTER, STICK_CENTER, 600},
  {SWITCH_B, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 200},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 75},
  {SWITCH_B, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 300},
  // Goes down the pokemon menu. Start of the loop (loop_start:13).
  {0, STICK_CENTER, STICK_MAX, 25},
  {0, STICK_CENTER, STICK_CENTER, 75},
  // loop_end:15
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 300},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 200},
  {SWITCH_A, STICK_CENTER, STICK_CENTER, BUTTON_DURATION},
  {0, STICK_CENTER, STICK_CENTER, 200},
  // Get on the bike!
  {SWITCH_PLUS, STICK_CENTER, STICK_CENTER, BUTTON_DURATION}
};

int phase = 0;
// The current point of execution in a step. 
int step_num = 0;
// Number of times that a loop has been executed. Used in ExecuteStepLoop and
// ExecuteStepPartialLoop.
int loop_num = 0;
int egg_slot = 0;

// Executes a sequence of steps.
void ExecuteStep(USB_JoystickReport_Input_t* const ReportData, Step_t* StepData, int size) {
  ReportData->Button |= StepData[step_num].Button;
  ReportData->LX = StepData[step_num].LX;
  ReportData->LY = StepData[step_num].LY;
  echoes = StepData[step_num].Duration;
  step_num++;
  if (step_num >= size) {
    step_num = 0;
    phase++;
  }
  return;
}

// Executes the entire sequence of steps for `num_its` iterations.
void ExecuteStepLoop(USB_JoystickReport_Input_t* const ReportData, Step_t* StepData, int size, int num_its) {
  ReportData->Button |= StepData[step_num].Button;
  ReportData->LX = StepData[step_num].LX;
  ReportData->LY = StepData[step_num].LY;
  echoes = StepData[step_num].Duration;
  step_num++;
  if (step_num >= size) {
    step_num = 0;
    loop_num++;
    if (loop_num >= num_its) {
      loop_num = 0;
      phase++;
    }
  }
  return;
}

// Repeats from step `loop_start` to `loop_end - 1` for `num_its` iterations.
// The other steps are executed once sequentially.
void ExecuteStepPartialLoop(USB_JoystickReport_Input_t* const ReportData, Step_t* StepData, int size, int loop_start, int loop_end, int num_its) {
  ReportData->Button |= StepData[step_num].Button;
  ReportData->LX = StepData[step_num].LX;
  ReportData->LY = StepData[step_num].LY;
  echoes = StepData[step_num].Duration;
  step_num++;
  if (step_num == loop_end && loop_num < num_its - 1) {
      step_num = loop_start;
      loop_num++;
  }
  if (step_num >= size) {
    step_num = 0;
    loop_num = 0;
    phase++;
  }
  return;
}

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t* const ReportData) {

	// Prepare an empty report
	memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
	ReportData->LX = STICK_CENTER;
	ReportData->LY = STICK_CENTER;
	ReportData->RX = STICK_CENTER;
	ReportData->RY = STICK_CENTER;
	ReportData->HAT = HAT_CENTER;

	// Repeat ECHOES times the last report
	if (echoes > 0)
	{
		memcpy(ReportData, &last_report, sizeof(USB_JoystickReport_Input_t));
		echoes--;
		return;
	}

  // Main Procedure
  if (phase == 0) {
    ExecuteStep(ReportData, SyncController, 8);
  } else if (phase == 1) {
    ExecuteStepPartialLoop(ReportData, GetEgg, 22, 13, 15, egg_slot + 1);
  } else if (phase == 2) {
    // The recall here is needed, otherwise the player will bump into an old man
    // on the bridge. Cannot be replaced with going down a few steps, because if
    // there is no egg available, the player would have already walked down a
    // little bit.
    ExecuteStep(ReportData, Recall, 11);
  } else if (phase == 3) {
    ExecuteStepLoop(ReportData, BikeBig, 2, 55);
  } else if (phase == 4) {
    ExecuteStep(ReportData, Recall, 11);
  }
  // Repeat Main Procedure
  if (phase == 5) {
    phase = 1;
    egg_slot = (egg_slot + 1) % 5;
  }

	// Prepare to echo this report
	memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t));
}
