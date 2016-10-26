/*
  Instructions:
 
  1. Create a Temboo account: http://www.temboo.com

  2. Retrieve your Temboo application details: http://www.temboo.com/account/applications

  3. Replace the values in the TembooAccount.h tab with your Temboo application details
 
  4. You'll also need a Gmail account. Update the placeholder Gmail address in the code 
     below with your own details.
     
     https://www.gmail.com
 
  5. Once you have a Gmail account, turn on 2-step authentication, and create an application-specific 
     password to allow Temboo to access your Google account: https://www.google.com/landing/2step/.
     
  6. After you've enabled 2-Step authentication, you'll need to create an App Password:
     https://security.google.com/settings/security/apppasswords
  
  7. In the "Select app" dropdown menu, choose "Other", and give your app a name (e.g., TembooApp).
  
  8. Click "Generate". You'll be given a 16-digit passcode that can be used to access your Google Account from Temboo.
 
  9. Copy and paste this password into the code below, updating the GMAIL_APP_PASSWORD variable
 
  10. Upload the sketch to your LaunchPad and open Energia's serial monitor

  NOTE: You can test this Choreo and find the latest instructions on our website: 
  https://temboo.com/library/Library/Google/Gmail/SendEmail
  
  You can also find an in-depth version of this example here:
  https://temboo.com/hardware/ti/send-an-email
  
  This example code is in the public domain.
*/

#include <Wire.h>
#include <BMA222.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Temboo.h>
#include "TembooAccount.h" // contains Temboo account information
                           // as described in the footer comment below
                           
/*** SUBSTITUTE YOUR VALUES BELOW: ***/

// note that for additional security and reusability, you could
// use #define statements to specify these values in a .h file.

// your Gmail username, formatted as a complete email address, e.g., "john.bonham@gmail.com"
const String GMAIL_USER_NAME = "";

// your application specific password (see instructions above)
const String GMAIL_APP_PASSWORD = "";

// the email address you want to send the email to, e.g., "johnpauljones@temboo.com"
const String TO_EMAIL_ADDRESS = "";


/***** Macros ******/
// most launchpads have a red LED
#define LED RED_LED

//see pins_energia.h for more LED definitions
//#define LED GREEN_LED

#define NUM_CAL_SAMPLES 20    /* # samples to use in calibration */
#define SAMPLE_PERIOD   500   /* sample rate in ms */
#define EMAIL_PERIOD    10000 /* cool down time between emails in ms */

#define THRESHOLD       12 /* Threshold beyond which movement is detected. Deviation in one dimension by more than 3 is classified as movement */
#define MOVEMENT_COUNT  (5000 / SAMPLE_PERIOD)  /* Door lock has to be in the same state for 5 sec for the state to become official */

#define BAUD_RATE    115200  /* Serial baud rate */
#define SETTLE_TIME  10000   /* Amount of time to wait between power up and calibration */

#define PACKET_LEN 255       /* max length of incoming packet */
#define NOTIFY "notify"      /* string to notify patio door board */
#define ACK "ack"            /* a string to send back  */
#define LOCAL_PORT 1743      /* local port to listen on */

/* Set the static IP address for this baord */
//IPAddress ip(192, 168, 0, 154);    

BMA222 mySensor;
int8_t nonMovingX, nonMovingY, nonMovingZ;    /* stationary parameters */
boolean shut = true;    /* Is door shut? */
uint32_t restCount = 0;  /* # readings while door is at rest and shut */
uint32_t moveCount = 0;  /* # readings while door is moving or open */

// Wifi client
WiFiClient client;

// Wifi server
char packetBuffer[255];          /* buffer to hold incoming packet */
char ReplyBuffer[] = ACK;      /* a string to send back  */
WiFiUDP Udp;

void setupWifi() {
  int wifiStatus = WL_IDLE_STATUS;

  // determine if the WiFi Shield is present.
  Serial.print("\n\nShield:");
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("FAIL");
    
    // if there's no WiFi shield, stop here.
    while(true);
  }
  else {
    Serial.println("OK\n");
  }

  /* unfortunately this prevents emails from being sent. So the front door would need to broadcast its notifications */
  //WiFi.config(ip);
   
  while(wifiStatus != WL_CONNECTED) {
    Serial.print("WiFi:");
    wifiStatus = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    if (wifiStatus == WL_CONNECTED) {
      Serial.println("OK");
    } else {
      Serial.println("FAIL");
    }
    delay(5000);
  }

  printWifiStatus();
           
  Serial.println("\nStarting UDP server...");
  Udp.begin(LOCAL_PORT);
}

void sendEmail(boolean shut) {

  Serial.println("Running SendAnEmail...");

  TembooChoreo SendEmailChoreo(client);

  // invoke the Temboo client
  // NOTE that the client must be reinvoked, and repopulated with
  // appropriate arguments, each time its run() method is called.
  SendEmailChoreo.begin();
  
  // set Temboo account credentials
  SendEmailChoreo.setAccountName(TEMBOO_ACCOUNT);
  SendEmailChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
  SendEmailChoreo.setAppKey(TEMBOO_APP_KEY);

  // identify the Temboo Library Choreo to run (Google > Gmail > SendEmail)
  SendEmailChoreo.setChoreo("/Library/Google/Gmail/SendEmail");
 
  // set the required Choreo inputs
  // see https://www.temboo.com/library/Library/Google/Gmail/SendEmail/ 
  // for complete details about the inputs for this Choreo

  // the first input is your Gmail email address    
  SendEmailChoreo.addInput("Username", GMAIL_USER_NAME);
  // next is your application specific password
  SendEmailChoreo.addInput("Password", GMAIL_APP_PASSWORD);
  // next is who to send the email to
  SendEmailChoreo.addInput("ToAddress", TO_EMAIL_ADDRESS);
  // then a subject line
  SendEmailChoreo.addInput("Subject", "ALERT!");

  if (shut) {
    // next comes the message body, the main content of the email   
    SendEmailChoreo.addInput("MessageBody", "Hey! The patio door is shut!");
  }
  else {
    // next comes the message body, the main content of the email   
    SendEmailChoreo.addInput("MessageBody", "Hey! The patio door is open!");
  }
  // tell the Choreo to run and wait for the results. The 
  // return code (returnCode) will tell us whether the Temboo client 
  // was able to send our request to the Temboo servers
  unsigned int returnCode = SendEmailChoreo.run();

  // a return code of zero (0) means everything worked
  if (returnCode == 0) {
      Serial.println("Success! Email sent!");
  } else {
    // a non-zero return code means there was an error
    // read and print the error message
    while (SendEmailChoreo.available()) {
      char c = SendEmailChoreo.read();
      Serial.print(c);
    }
  } 
  SendEmailChoreo.close();
}

void setup()
{
  Serial.begin(BAUD_RATE);

  // initialize the digital pin as an output.
  pinMode(LED, OUTPUT); 
  
  /* Turn on LED during setup and calibration */
  digitalWrite(LED, HIGH);
  
  /* Setting up wifi */
  setupWifi();
  
  /* Setup BM222 */
  mySensor.begin();
  uint8_t chipID = mySensor.chipID();
  Serial.print("chipID: ");
  Serial.println(chipID);
  
  calibrate();
  
  /* Setup done. Turn off LED. */
  digitalWrite(LED, LOW);
}

void calibrate()
{
  uint8_t i;
  int32_t accX = 0;
  int32_t accY = 0;
  int32_t accZ = 0;
  int8_t data;
  
  Serial.println("Calibrating...");
  
  sleep(SETTLE_TIME); //wait a few seconds for things to settle 

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

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void serviceUdpPackets()
{
  /* if there's data available, read a packet */
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remoteIp = Udp.remoteIP();
    Serial.print(remoteIp);
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, PACKET_LEN);
    if (len > 0) {
      packetBuffer[len] = 0;
      Serial.println("Contents:");
      Serial.println(packetBuffer);

      String receivedStr = String(packetBuffer);

      if (receivedStr.compareTo(NOTIFY) == 0) {
        // send a reply, to the IP address and port that sent us the packet we received
        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(ReplyBuffer);
        Udp.endPacket();
  
        if (!shut) {
          Serial.println("Sending notification email!");
          sendEmail(shut);
          sleep(EMAIL_PERIOD);
          /* flush any duplicate packets */
          while (Udp.parsePacket() > 0) {
            sleep(SAMPLE_PERIOD);
          }
        }
      }
    }
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

  serviceUdpPackets();
    
  sleep(SAMPLE_PERIOD);  
}
