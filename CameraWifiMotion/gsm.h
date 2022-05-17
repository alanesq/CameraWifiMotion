/**************************************************************************************************
 *
 *                                    GSM module - 17May22

 *             part of the BasicWebserver sketch - https://github.com/alanesq/BasicWebserver
 *             Note:  Not yet fully working - will need modifying for esp32 as no software serial
 *
 *  See:  https://lastminuteengineers.com/a6-gsm-gprs-module-arduino-tutorial/
 *        https://randomnerdtutorials.com/sim900-gsm-gprs-shield-arduino/
 *        AT commands: https://radekp.github.io/qtmoko/api/atcommands.html
 *
 *
 *  Note:
 *        takes a few seconds for GSM module to connect to network before it will send any replys
 *        needs good power supply (my sim800 seems to need 5v?)
 *        RST low to turn off
 *
 *        If the GSM module keeps turning off it is probably the power supply can't supply enough current
 *
 *        If you send it a text message containing "send sms" it will send a reply message
 *
 *        A standard received sms will look like this:
 *                                                      +CIEV: "MESSAGE",1
 *
 *                                                      +CMT: "+447812343449",,"2021/01/20,10:46:09+00"
 *                                                      This is a test text message
 *
 *        A message from GiffGaff will look like this (i.e. no date or number info):
 *                                                      +CIEV: "MESSAGE",1
 *
 *                                                      +CMT: "Hi, that text was charged from your giffgaff credit balance. Your credit balance is now ⸮2.40.
 *
 *
 **************************************************************************************************


 Notes:

    Mainly designed for use with the Sim800 GSM board
        Note: it requires an odd voltage and allegedly up to 2 amps - I have found running it from 5 volts
              going through a diode to drop the voltage along with a good sized smoothing capacitor seems to work ok.

    You can issue AT commands to the GSM module via Serial Monitor

    To set custom actions on incoming data from the GSM module see dataReceivedFromGSM()
        e.g. respond to incoming sms message or incoming data requested via requestWebPageGSM()

    For a Sim to use in your GSM module I recommend GiffGaff, they are good value and do not seem to get disconnected if you do not top
        them up very often.
        If you top it up every 3 months (£10 min.) then text messages between Giffgaff phones are not charged for.

    Some of the GSM boards require an odd voltage (between 3.4 and 4.4volts), The easiest way to deal with this
        would be to use 5volts and connect it to the GSM board via a diode as the diode will give a 0.6volt drop.
        I have not seen this suggested anywhere, I don't know if there is some reason why?




Common Sim800 AT commands:

    SIM800 routine
        AT	        Check interface
        AT+CPIN?        Check if SIM is unlocked
    Initialize phone
        AT+CREG?	Check network status
        AT+CFUN=0       Minimal phone functionality (Sim Reset)
        AT+CFUN=1       Full functionality
        AT+CSQ	        Check signal
    GPRS Setup
        AT+CGATT?	Check if connected to GPRS
        AT+CGATT=0	Detach from GPRS network
        AT+CGATT=1	Attach to GPRS network
        AT+CIPSHUT	Reset IP session
        AT+CIPSTATUS	Check if IP stack is initialized
        AT+CIPMUX=0	Setting up single connection mode
    Ping Tests
        AT+CSTT=”internet”      Ping request
        AT+CIICR	Bring up wireless connection
        AT+CIFSR	Get local IP address
        AT+CIPSTART=”TCP”,”url”	    Start connection
        AT+CIPSEND	Request initiation for data sending
    Bearer Configure
        AT+SAPBR=3,1,”CONTYPE”,”GPRS”	Configure bearer profile 1
        AT+SAPBR=3,1,”APN”,”internet”	Set “internet” as APN. Varies per different network
        AT+SAPBR=1,1	To open a GPRS context
        AT+SAPBR=2,1	To query the GPRS context
        AT+SAPBR=0,1	To close GPRS context
    Get Location
        AT+CLBS=?	Base station test command
        AT+CLBSCFG=0,1	Get customer ID
        AT+CLBSCFG=0,2	Get Times have use positioning command
        AT+CLBS=1,1	Get current longitude, latitude and precision
        AT+CLBS=3,1	Get access times
        AT+CLBS=4,1	Get current longitude, latitude, precision and date time
    Network Time Synchronize
        AT+CNTPCID=1	Set NTP use bear profile 1
        AT+CNTP=”time.upd.edu.ph”,32	Set NTP service URL and local time zone
        AT+CNTP	Start sync network time
        AT+CCLK?	Query local time
    HTTP Request
        AT+HTTPINIT	Initialize HTTP service
        AT+HTTPPARA=”CID”,1	Set parameters for HTTP session
        AT+HTTPPARA=”REDIR”,1	Set to enable redirect
        AT+HTTPPARA=”URL”,”http://m.smart.com.ph”	Set the URL
        AT+HTTPACTION=0	Start the HTTP session
        AT+HTTPREAD	Read the data of the HTTP server
        AT+HTTPTERM	Terminate HTTP service
        AT+HTTPSTATUS?	Check HTTP status



*/

 //            --------------------------- settings -------------------------------


const String phoneNumber = "+4411111111";          // phone number to send sms messages to

const String GSM_APN = "giffgaff.com";             // APN for mobile data

const int GSMresetPin = -1;                        // GSM reset pin (-1 = not used)
bool GSMresetPinActive = 1;                        // reset active state (i.e. 1 = high to reset device)

const int TxPin = D5;                              // Tx pin
const int RxPin = D6;                              // Rx pin

int checkGSMmodulePeriod = 600;                    // how often to check GSM module is still responding ok (seconds)

int checkGSMdataPeriod = 1;                        // how often to check for incoming data from GSM module (seconds)

const int GSMbuffer = 512;                         // buffer size for incoming data from GSM module


//            --------------------------------------------------------------------


// forward declarations
  String contactGSMmodule(String);
  void sendSMS(String , String);
  bool checkGSMmodule(int);
  bool resetGSM(int);
  void GSMSetup();
  bool requestWebPageGSM(String, String, int);
  void dataReceivedFromGSM();


#include <SoftwareSerial.h>    // Note: the esp32 has a second hardware serial port you can use instead of SoftwareSerial

uint32_t checkGSMmoduleTimer = millis();    // timer for periodic gsm module check
uint32_t checkGSMdataTimer = millis();      // timer for periodic check for incoming data from GSM module

//Create software serial object to communicate with GSM module
  SoftwareSerial GSMserial(TxPin, RxPin);



// ----------------------------------------------------------------
//                     -act on any incoming data
// ----------------------------------------------------------------
// periodic check for any incoming data from gms module - called from GSMloop()

void dataReceivedFromGSM() {

    String reply = contactGSMmodule("");  // check gsm module for any incoming data on serial
    if (reply == "") return;   // no incoming data from GSM module


    // check for an incoming SMS message
      if (reply.indexOf("+CIEV: \"MESSAGE\"") >= 0) {       // search for:     +CIEV: "MESSAGE"
        int pos = reply.indexOf("+CMT: ");                  // search for:     +CMT:
            if (pos >= 0) {
              // a sms has been received
                String smsMessage = reply.substring(pos + 6);
                if (serialDebug) {
                  Serial.println("SMS message received:");
                  Serial.println(smsMessage);
                }
            } else if (serialDebug) Serial.println("Error in received SMS message");
      }


//    // send a test sms message if "send sms" in a received sms message
//      if (reply.indexOf("send sms") >= 0) sendSMS(phoneNumber, "this is a test message from the gsm demo sketch");


//    // request a web page via GSM data if "get web" received in a sms message
//    //   see comments below requestWebPageGSM() for example of a reply
//      if (reply.indexOf("get web") >= 0) requestWebPageGSM("http://alanesq.eu5.net/temp/q.txt");

}


// ----------------------------------------------------------------
//                       -Setup GSM board
// ----------------------------------------------------------------
//  setup for GSM board - called from setup

void GSMSetup() {

  // configure reset pin
    if (GSMresetPin != -1) {
        digitalWrite(GSMresetPin, !GSMresetPinActive);     // set to not active
        pinMode(GSMresetPin, OUTPUT);
    }

  //Begin serial communication with GSM module
    GSMserial.begin(9600, SWSERIAL_8N1, D5, D6, false, GSMbuffer); while(!GSMserial) delay(200);

  // check GSM module is responding (up to 40 attempts, this will set the flag 'GSMconnected')
    checkGSMmodule(30);

  if (GSMconnected) {
    contactGSMmodule("ATI");             // Get the module name and revision
    if (serialDebug) Serial.println("GSM setup completed OK");
  } else {
    if (serialDebug) Serial.println("ERROR: GSM module is not responding");
  }

}   // setupGSM


// ----------------------------------------------------------------
//              -loop, routine activites for GSM module
// ----------------------------------------------------------------
// called from 'Loop'

void GSMloop() {

  if (GSMconnected) {

    // periodic check that GSM module is still responding
      if ((unsigned long)(millis() - checkGSMmoduleTimer) >= (checkGSMmodulePeriod * 1000) ) {
          checkGSMmoduleTimer = millis();
          if (!checkGSMmodule(2)) {
            if (serialDebug) Serial.println("ERROR: GSM module has stopped responding");
          }
      }

    // periodic check for any incoming data on serial or from gsm module
      if ((unsigned long)(millis() - checkGSMdataTimer) >= (checkGSMdataPeriod * 1000) ) {
        checkGSMdataTimer = millis();
        dataReceivedFromGSM();
      }

  }  // GSMconnected

}  // GSMloop


// ----------------------------------------------------------------
//                        -reset GSM module
// ----------------------------------------------------------------

bool resetGSM(int GSMcheckRetries) {

  if (GSMresetPin == -1) {
    if (serialDebug) Serial.println("Unable to reset GSM device as no reset pin defined");
    return 0;
  }

  if (serialDebug) Serial.println("Resetting GSM device");

  digitalWrite(GSMresetPin, GSMresetPinActive);    // reset active
  delay(1000);
  digitalWrite(GSMresetPin, !GSMresetPinActive);    // restart device
  delay(2000);

  checkGSMmodule(GSMcheckRetries);
  return GSMconnected;
}


// ----------------------------------------------------------------
//                -check gsm module is responding
// ----------------------------------------------------------------
// sets the flag 'GSMconnected'

bool checkGSMmodule(int maxTries) {

  if (serialDebug) Serial.println("Checking GSM module is responding");
  bool GSMconnectedCurrent = GSMconnected;    // store current GSM status

  String reply;
  GSMconnected = 0;
  while (GSMconnected == 0  && maxTries > 0) {
    reply = contactGSMmodule("AT");
    if (reply.indexOf("OK") >= 0) GSMconnected = 1;
    maxTries--;
  }

  if (serialDebug){
    if (!GSMconnected) Serial.println("No response received from GSM device");
    else Serial.println("GSM device responding ok");
  }

  // if GSM wasn't connected but now is send some configuration
  //   see:  https://oldlight.wordpress.com/2009/06/16/tutorial-using-at-commands-to-send-and-receive-sms/
  if (GSMconnected && !GSMconnectedCurrent) {
    if (serialDebug) Serial.println("Configuring incoming SMS");
    contactGSMmodule("AT+CNMI=1,2,0,0,0");           // Set module to send SMS data to serial upon receipt
    contactGSMmodule("AT+CMGF=1");                   // format sms as text
  }

  return GSMconnected;

}   // GSMmodule


// ----------------------------------------------------------------
//                       send a sms message
// ----------------------------------------------------------------

void sendSMS(String SMSnumber, String SMSmessage) {

  //const int sdel = 1000;            // delay between comands
  String reply;

  if (serialDebug) Serial.println("Sending SMS to '" + SMSnumber + "', message = '" + SMSmessage + "'");

  reply = contactGSMmodule("AT+CMGF=1");                         // put in to sms mode
  //delay(sdel);

  reply = contactGSMmodule("AT+CMGS=\"" + SMSnumber + "\"");     // e.g. "AT+CMGS=\"+ZZxxxxxxxxxx\"" - change ZZ with country code and xxxxxxxxxxx with phone number to sms
  //delay(sdel);

  reply = contactGSMmodule(SMSmessage);                          // the message to send

  GSMserial.write(26);                                           // Ctrl Z

}
/*

Response when text was sent ok  (the 4 changes):
                              +CMGS: 4

                              OK
*/



// ----------------------------------------------------------------
//                   Request web page via GSM
// ----------------------------------------------------------------
// see: https://predictabledesigns.com/the-sim800-cellular-module-and-arduino-a-powerful-iot-combo/

bool requestWebPageGSM(String ip, String page, int port) {

    const int sdel = 1000;            // delay between commands
    ip = "http://" + ip;
    String reply;

    if (serialDebug) Serial.println("Requesting web page via GSM '" + ip + ":" + String(port) + page + "', message = '");

    // Sim800:

        // reply = contactGSMmodule("AT+CSQ");                                      // Check for signal quality

        reply = contactGSMmodule("AT+CGATT=1");                                     // Attach to a GPRS network
        //delay(sdel);

        reply = contactGSMmodule("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");              // Configure bearer profile 1
        //delay(sdel);

        reply = contactGSMmodule("AT+SAPBR=3,1,\"APN\",GSM_APN");                   // APN for phone network
        //delay(sdel);

        reply = contactGSMmodule("AT+SAPBR=1,1");                                   // To open a GPRS context  (slight delay then OK)
        //delay(sdel);

        reply = contactGSMmodule("AT+HTTPINIT");                                    // Init HTTP service
        //delay(sdel);

        reply = contactGSMmodule("AT+HTTPPARA=\"CID\",1");                          // Set parameters for HTTP session
        //delay(sdel);

        reply = contactGSMmodule("AT+HTTPPARA=\"REDIR\",1");                        // Auto redirect
        //delay(sdel);

        reply = contactGSMmodule("AT+HTTPPARA=\"URL\",\"" + ip + ":" + String(port) + page + "\"");     // send web site URL
        //delay(sdel);

        reply = contactGSMmodule("AT+HTTPACTION=0");                                // Get the web page -  sfter delay responds "+HTTPACTION: 0,200,9"
        delay(sdel);

        // Note: All above commands should get a response of an echo of the command sent on the first line and "OK" on the second line
        //       The last line will get a second response a few seconds later,     e.g. blank first line, second line = "+HTTPACTION: 0,200,9"

        reply = contactGSMmodule("AT+HTTPREAD");                                    // Read the data of the HTTP server - reply

        // Note: See below for example reply from above command

        // check if ok received
          if (reply.indexOf("OK") >= 0) {
            if (serialDebug) Serial.println("Request sent ok");
            return 1;
          } else {
            if (serialDebug) Serial.println("Error sending request");
            return 0;
          }


  // A6
  //    reply = contactGSMmodule("AT+HTTPGET=\"" + URL + "\"");                     // request URL

}
/*

 reply example - A6:
                  +HTTPRECV:HTTP/1.1 200 OK
                  Date: Thu, 21 Jan 2021 16:49:46 GMT
                  Server: Apache
                  Last-Modified: Thu, 21 Jan 2021 07:37:32 GMT
                  ETag: "9-5b96424ff7617"
                  Accept-Ranges: bytes
                  Content-Length: 9
                  Keep-Alive: timeout=4, max=90
                  Connection: Keep-Alive
                  Content-Type: text/plain

                  it works


 reply example - Sim800:
                  AT+HTTPREAD

                  +HTTPREAD: 9
                  it works

                  OK

*/


// ----------------------------------------------------------------
//                 exchange data with GSM module
// ----------------------------------------------------------------
// GSMcommand is sent to GSM module
// If data being lost try increasing 'GSMbuffer' size

String contactGSMmodule(String GSMcommand) {

  const int delayTime = 600;                  // length of delays (ms)
  char replyStore[GSMbuffer + 1];             // store for reply from GSM module
  int received_counter = 0;                   // number of characters which have been received

  // forward any incoming data an serial to the gsm module
    if (Serial.available()) {
      delay(delayTime);      // make sure full message has time to come in first
      while (Serial.available()) {
          GSMserial.write(Serial.read());
      }
    }

  // if a command was supplied send it to GSM module
    if (GSMcommand != "") {
      if (serialDebug) Serial.println("Sending command to GSM module: '" + GSMcommand +"'");
      GSMserial.println(GSMcommand);
      delay(delayTime);
    }

  // if any data coming in from GSM module copy it to a string
    if (GSMserial.available()) delay(delayTime);    // make sure full message has time to come in first
    while(GSMserial.available() && received_counter < GSMbuffer) {
        char tstore = char(GSMserial.read());
        replyStore[received_counter] = tstore;      // Forward what Software Serial received to a String
        received_counter+=1;
    }
    replyStore[received_counter] = 0;               // end of string marker

  if (received_counter > 0 && serialDebug) {
    Serial.println("--------------- Data received from gsm module --------------");
    Serial.print(replyStore);
    Serial.println("------------------------------------------------------------");
  }

//  if (!strstr(replyStore, "OK")) {
//    // No valid reply received
//      return "ERROR";
//  }

  return replyStore;
}


// --------------------------------------------------------------------------

// end
