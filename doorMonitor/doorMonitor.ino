#include <Wire.h>
#include <BMA222.h>

// most launchpads have a red LED
#define LED RED_LED

//see pins_energia.h for more LED definitions
//#define LED GREEN_LED

#define NUM_CAL_SAMPLES 20    /* # samples to use in calibration */
#define SAMPLE_PERIOD   500   /* sample rate in ms */

#define THRESHOLD       12 /* Threshold beyond which movement is detected. Deviation in one dimension by more than 3 is classified as movement */
#define MOVEMENT_COUNT  (5000 / SAMPLE_PERIOD)  /* Door lock has to be in the same state for 5 sec for the state to become official */

#define BAUD_RATE    115200  /* Serial baud rate */
#define SERIAL_DELAY 5000    /* Amount of time to wait for serial monitor to be opened */
#define SETTLE_TIME  15000   /* Amount of time to wait between power up and calibration */


BMA222 mySensor;
int8_t nonMovingX, nonMovingY, nonMovingZ;
boolean shut = false;
uint32_t restCount = 0;
uint32_t moveCount = 0;

void setup()
{
  Serial.begin(BAUD_RATE);
  sleep(SERIAL_DELAY);   /* Allow time for serial monitor to be opened */
  
  /* Setup BM222 */
  mySensor.begin();
  uint8_t chipID = mySensor.chipID();
  Serial.print("chipID: ");
  Serial.println(chipID);
  
  // initialize the digital pin as an output.
  pinMode(LED, OUTPUT); 
  
  calibrate();
}

void calibrate()
{
  uint8_t i;
  int32_t accX = 0;
  int32_t accY = 0;
  int32_t accZ = 0;
  int8_t data;
  
  Serial.println("Calibrating...");
  digitalWrite(LED, HIGH);   // turn the LED on (HIGH is the voltage level)
  
  sleep(SETTLE_TIME - SERIAL_DELAY); //wait a few more seconds for things to settle 

  /* Compute stationary non-moving gravity vector */
  for (i = 0; i < NUM_CAL_SAMPLES; i++) {
    accX += mySensor.readXData();
    accY += mySensor.readYData();
    accZ += mySensor.readZData();
    sleep(SAMPLE_PERIOD);
  }

  accX += (NUM_CAL_SAMPLES / 2);  /* rounding */
  accY += (NUM_CAL_SAMPLES / 2);  /* rounding */
  accZ += (NUM_CAL_SAMPLES / 2);  /* rounding */

  nonMovingX = accX / NUM_CAL_SAMPLES;
  nonMovingY = accY / NUM_CAL_SAMPLES;
  nonMovingZ = accZ / NUM_CAL_SAMPLES;
  
  Serial.print("nonmovingX =");
  Serial.print(nonMovingX);
  Serial.print("nonmovingY =");
  Serial.print(nonMovingY);
  Serial.print("nonmovingZ =");
  Serial.println(nonMovingZ);
  
  /* Initial averaging done, seek direction of door opening */
  digitalWrite(LED, LOW);    // turn the LED off by making the voltage LOW
  
  Serial.println("Done calibration");
}

/* True if we moved between the last two samples */
boolean detectMovement(int8_t X, int8_t Y, int8_t Z)
{
  int8_t deltaX = X - nonMovingX;  
  int8_t deltaY = Y - nonMovingY;  
  int8_t deltaZ = Z - nonMovingZ;  

  if (((deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ)) < THRESHOLD) {
    return false;
  }
  else {
    return true;
  }
}


void loop()
{
  int8_t dataX, dataY, dataZ;
  
  dataX = mySensor.readXData();
  //Serial.print("X: ");
  //Serial.print(dataX);

  dataY = mySensor.readYData();
  //Serial.print(" Y: ");
  //Serial.print(dataY);

  dataZ = mySensor.readZData();
  //Serial.print(" Z: ");
  //Serial.println(dataZ);
  
  if (!detectMovement(dataX, dataY, dataZ)) {
    moveCount = 0;
    if (restCount < MOVEMENT_COUNT) {
      restCount++;
      Serial.print(" restCount= ");
      Serial.println(restCount);
      digitalWrite(YELLOW_LED, LOW);   // turn the LED off
    }
    else {
      /* when restCount reaches REST_COUNT, turn LED off as the door is officially closed */
      if (!shut) {
        digitalWrite(LED, LOW);   // turn the LED off
        Serial.println("door is shut");
        shut = true;
      }
    }
  }
  else {
    /* If door is moving or open, don't bother computing anything. */
    restCount = 0;
    if (moveCount < MOVEMENT_COUNT) {
      moveCount++;
      Serial.print(" moveCount= ");
      Serial.println(moveCount);
      digitalWrite(YELLOW_LED, HIGH);   // turn the LED on
    }
    else {
      /* when moveCount reaches MOVE_COUNT, turn LED off as the door is officially opened */
      if (shut) {
        shut = false;
        Serial.println("door is opened");
        digitalWrite(LED, HIGH);   // turn the LED on
      }
    }
  }
  
  sleep(SAMPLE_PERIOD);  
}
