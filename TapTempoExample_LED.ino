#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

// Assign pins

//DigitalPin Assignments
const unsigned int tapSwitch = 1; // Tap tempo switch on PB1/ Pin 6
const unsigned int ledOut = 2; // LED indicator out on PB2/Pin 7

//Set up delay time parameters
uint8_t minTime = 50; // shortest allowable delay time, in ms
unsigned int maxTime = 650; // longest allowable delay time, in ms
uint8_t timeStep = 5; // The amount of time between divisions (in ms) over the range of [minTime:maxTime]
unsigned int tapTime; //set to 0 so that any valid tap tempo time is differentiated
unsigned int delayTime; //set this to the default in setup()
unsigned int prevDelayTime; //set this to the default in setup()

// Initiate the tap tempo button stuff
uint8_t buttonState;
uint8_t lastButtonState = LOW; // Previous state of tap tempo switch
uint8_t tapStatus = LOW; // Current status of the tap tempo switch
uint8_t updateTapTempo = 0;
unsigned int currentMillis; // Used for debouncing tap tempo switch

unsigned long lastDebounceTime = 0; // initiate the debounce time counter
uint8_t debounceDelay = 50; // How long we enforce a debounce, ms

uint8_t prevTaps = 0; // How many taps we have in our history
unsigned int tapTimeout = 1000; // 1s tap timeout
uint8_t maxTaps = 10; // Max number of taps to keep in history
unsigned int prevTimes [10]; // Array for storing the tap periods
uint8_t useTap = 0; // This is used to determine if we have enough taps to calculate the tap period
unsigned int prevTapDelay = 0; // This is used for debouncing the tap tempo switch


// Set up LED blinking parameters
unsigned int prevMillis; // Used for keeping track of LED blinking
unsigned int maxLEDTime = 200; //Maximum LED blink on duration in ms
unsigned int currLEDState = LOW; // LED starts off off
unsigned int currLEDOffInterval; // How long the LED has been off
unsigned int currLEDOnInterval; // How long the LED has been on
unsigned int blinkDuration; // How long should the LED be on
unsigned int currVoltInterval; // How long have we held the current voltage level
uint8_t updateLEDInterval = 1;

void setup() {
  //Define what each pin is
  pinMode(tapSwitch, INPUT_PULLUP);
  pinMode(ledOut, OUTPUT);

  //Set up the initial state of the pins
  digitalWrite(tapSwitch, LOW);
  digitalWrite(ledOut, LOW);

  prevMillis = millis();
  delayTime = 250;
  prevDelayTime = delayTime;
  tapTime = delayTime;
  updateLED();
  
  //Set up an ISR for the tap switch pin so that it reacts instantly
  //and to reduce loop execution time with determining multiplier
  GIMSK = 0b00100000; //enable pin change interrupts
  PCMSK = 0b00000010; //enable PB1 as pin change interruptible
  sei(); //start interrupt service
}



void loop() {
  //Check to see if tap tempo is used only if the PCInt says we need it
  if(updateTapTempo){
    checkTapTempo();
  }

  //Update the delay time based on above, if needed
  updateDelayTime();

  // What time is it now?
  currentMillis = millis();

  //Update the LED blink rate
  //Blink LED for 1/2 period with a max illumination of maxLEDTime ms per period
  updateLED();

}


//Interrupt handling
ISR (PCINT0_vect) {

  if(digitalRead(tapSwitch)){//Tap switch was pressed, set flag to update tap tempo
    updateTapTempo = 1;
  }
  
}



void checkTapTempo() {
  
  //Check to see if the tap tempo switch has actually been pressed
  switchDebounce();

  if (tapStatus == HIGH) {

    tapStatus = LOW;
    //Check to see if we already have a tap tempo history. If so, add this to
    //the history. If not, start a new count.
    if (prevTaps > 0) {
      int currTime = millis();
      int currDelay = currTime - prevTapDelay;
      // Check to make sure we didn't time out
      if (currDelay < tapTimeout) {
        //Set the flag for using tap tempo
        useTap = 1;

        // Create the temp array for storing times in
        unsigned int newPrevTimes [maxTaps];

        if (prevTaps < maxTaps) {

          //Fill up the new array with all the old values first
          for (int k = 0; k < prevTaps - 1; k++) {
            newPrevTimes[k] = prevTimes[k];
          }

          //Then add in the new value at the end
          newPrevTimes[prevTaps - 1] = currDelay;
          prevTaps++;

        } // End if prevTaps < maxTaps
        
        for (int nTime = 0; nTime < maxTaps; nTime++) {
          prevTimes[nTime] = newPrevTimes[nTime];
        }

      } // End if currDelay < tapTimeout
      else {
        //If we timeout, reset the counter and zero out the tempo array
        prevTaps = 1;

        for (int i = 0; i < maxTaps; i++) {
          prevTimes[i] = 0;
        }

        useTap = 0;
      } // End if tap has timed out
    } // End if prevTaps > 0
    // If we do not have any previous taps (first tap after timeout)
    else {
      prevTaps = 1;

      for (int i = 0; i < maxTaps; i++) {
        prevTimes[i] = 0;
      }

      useTap = 0;
    }

    if (useTap == 1 && prevTaps > 2) {
      //Calculate the average polling time, including the multiplier and the random switch
      int sum, loop, numVals;
      float avg;

      sum = avg = 0;
      numVals = 0;

      for (loop = 0; loop < prevTaps - 1; loop++) {
        if (prevTimes[loop] != 0) {
          sum += prevTimes[loop];
          numVals++;
        }
      }
      avg = (float)sum / numVals;
      tapTime = round(avg);

      //Don't let it go longer than our max delay time
      if (tapTime > maxTime) {
        tapTime = maxTime;
      }
      
    }
    else {
      //If we don't have the information to produce a tap tempo, stick with what we have
    }
    prevTapDelay = millis();
  }

}//End of checkTapTempo()



//Code for debouncing tap tempo switch
void switchDebounce() {
  int reading = digitalRead(tapSwitch);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (reading != buttonState) {

      buttonState = reading;

      if (buttonState == HIGH) {
        tapStatus = HIGH;
      }
    }
  }
  lastButtonState = reading;
}//End of switchDebounce()



void updateDelayTime() {
  //If we are more than timeStep ms off or changing multiplier, update the delayTime
  if ((abs(tapTime - prevDelayTime) >= timeStep)) {
    
      delayTime = tapTime; // We didn't update multiplier, so the base delay time was changed
      prevDelayTime = tapTime; // Update prevDelayTime to the new value
      updateLEDInterval = 1; // Set flag to change LED flashing time
  }
}//End of updateDelayTime


//Code for LED flashing update
void updateLED() {

  // Figure out what the on and off times are for LED flashing
  if (updateLEDInterval) {
    updateLEDInterval = 0;

    if (delayTime / 2 >= maxLEDTime) {
      currLEDOnInterval = maxLEDTime;
    }
    else {
      currLEDOnInterval = round(delayTime / 2);
    }
    currLEDOffInterval = round(delayTime - currLEDOnInterval);
  }
  

  //Check to see if we have completed the LED on or off interval and change if we have
  if (currLEDState == LOW) {
    if (currentMillis - prevMillis >= currLEDOffInterval) {
      currLEDState = HIGH;
      prevMillis += currLEDOffInterval;
      digitalWrite(ledOut, HIGH);
    }
  }

  if (currLEDState == HIGH) {
    if (currentMillis - prevMillis >= currLEDOnInterval) {
      currLEDState = LOW;
      prevMillis += currLEDOnInterval;
      digitalWrite(ledOut, LOW);
    }
  }

}//End of updateLED()
