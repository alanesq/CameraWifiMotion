/**************************************************************************************************
 *
 *      Wifi / NTP Connections using WifiManager - 13Oct21
 *      
 *      part of the BasicWebserver sketch - https://github.com/alanesq/BasicWebserver
 *             
 *      Set up wifi for either esp8266 or esp32 plus NTP (network time)
 *      
 *      see:  https://nodemcu.readthedocs.io/en/release/modules/wifi/
 *                    
 *      Libraries used: 
 *                      ESP_Wifimanager - https://github.com/khoih-prog/ESP_WiFiManager
 *                      TimeLib
 *                      ESPmDNS
 *                    
 *  
 **************************************************************************************************/



// **************************************** S e t t i n g s ****************************************

    
    // Configuration Portal (Wifimanager)
      String AP_SSID = "ESPPortal";
      String AP_PASS = "12345678";    


//     mDNS name
//       const String mDNS_name = "esp32";
//       const String mDNS_name = stitle;                                         // use sketch title
      


// *************************************************************************************************


// forward declarations
  void startWifiManager();
  String currentTime();
  bool IsBST();
  void sendNTPpacket();
  time_t getNTPTime();
  String requestWebPage(String, String, int, int);
  
  
// ----------------------------------------------------------------
//                              -Startup
// ----------------------------------------------------------------
  
// wifi for esp8266 / esp32
  #if defined ESP32
    #include <esp_wifi.h>
    #include <WiFi.h>
    #include <WiFiClient.h>
    #include <WebServer.h>
    #define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
    WebServer server(ServerPort);
    //#include <ESPmDNS.h>                // see https://github.com/espressif/arduino-esp32/tree/master/libraries/ESPmDNS      
  #elif defined ESP8266
    #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
    //needed for library
    #include <DNSServer.h>
    #include <ESP8266WebServer.h>
    #define ESP_getChipId()   (ESP.getChipId())
    ESP8266WebServer server(ServerPort);
    //#include <ESP8266mDNS.h>
  #else
      #error "This sketch only works with the ESP8266 or ESP32"
  #endif
 
  #include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager   


// Time from NTP server
//  from https://raw.githubusercontent.com/RalphBacon/No-Real-Time-Clock-RTC-required---use-an-NTP/master
  #include <TimeLib.h>
  #include <WiFiUdp.h>                          // UDP library which is how we communicate with Time Server
  const uint16_t localPort = 8888;              // Just an open port we can use for the UDP packets coming back in
  const char timeServer[] = "pool.ntp.org"; 
  const uint16_t NTP_PACKET_SIZE = 48;          // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[NTP_PACKET_SIZE];           // buffer to hold incoming and outgoing packets
  WiFiUDP NTPUdp;                               // A UDP instance to let us send and receive packets over UDP
  const uint16_t timeZone = 0;                  // timezone (0=GMT)
  const String DoW[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  const uint16_t _resyncSeconds = 7200;         // How often to resync the time (under normal conditions) 7200 = 2 hours
  const uint16_t _resyncErrorSeconds = 300;     // How often to resync the time (under error conditions) 300 = 5 minutes


// ----------------------------------------------------------------
//                 -wifi initialise  (called from 'setup')
// ----------------------------------------------------------------

void startWifiManager() {

  // Connect to Wifi using WifiManager
    ESP_WiFiManager ESP_wifiManager("AutoConnectAP");
    ESP_wifiManager.setConfigPortalTimeout(120);
    ESP_wifiManager.setDebugOutput(true);   

  // get stored wifi settings
    String Router_SSID = ESP_wifiManager.WiFi_SSID();    
    String Router_Pass = ESP_wifiManager.WiFi_Pass();
    if (Router_SSID == "") if (serialDebug) Serial.println("There are no wifi settings stored");

  // try connecting to wifi
    if (serialDebug) Serial.println("Connecting to wifi using WifiManager");
    WiFi.begin(Router_SSID.c_str(), Router_Pass.c_str());

  // if unable to connect to wifi start config portal  
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      if (serialDebug) Serial.println("Unable to connect to WiFi - starting Wifimanager config portal");
      if ( !ESP_wifiManager.startConfigPortal(AP_SSID.c_str(), AP_PASS.c_str()) ) {
        if (serialDebug) Serial.println("Not connected to WiFi - rebooting");
        delay(1000);
        ESP.restart();  
        delay(5000);           // restart will fail without this delay
      }
    }    

  // finished connecting to wifi (it should be connected at this point)
    if (WiFi.status() == WL_CONNECTED) {
      if (serialDebug) Serial.print("connected to wifi. Local IP: ");
      if (serialDebug) Serial.println(WiFi.localIP());
      wifiok = 1;  
    } else {
      if (serialDebug) Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
    }  
          
//  // Set up mDNS responder:
//    if (serialDebug) Serial.println( MDNS.begin(mDNS_name.c_str()) ? "mDNS responder started ok" : "Error setting up mDNS responder" );

  // start NTP (Time)
    NTPUdp.begin(localPort); 
    setSyncProvider(getNTPTime);              // the function that gets the time from NTP
    setSyncInterval(_resyncErrorSeconds);     // How often to re-synchronise the time (in seconds) 
         
}  // startwifimanager


// ----------------------------------------------------------------
//          -Return current time and date as a string
// ----------------------------------------------------------------

String currentTime(){

   time_t t=now();     // get current time 
   
   if (year(t) < 2021) return "Time Unknown";

   if (IsBST()) t+=3600;     // add one hour if it is Summer Time

   String ttime = String(hour(t)) + ":" ;          // hours
   
   int tmin = minute(t);
   if (tmin < 10) ttime += "0";                    // minutes
   ttime += String(tmin) + ":";
   
   int tsec = second(t);
   if (tsec < 10) ttime += "0";                    // seconds
   ttime += String(tsec);   
   
   ttime += " " + DoW[weekday(t)-1] + "_";                                            // day of week
   ttime += String(day(t)) + "-" + String(month(t)) + "-" + String(year(t)) + " ";    // date

   return ttime;
   
}  // currentTime


//-----------------------------------------------------------------------------
//                           -British Summer Time check
//-----------------------------------------------------------------------------
// returns true if it is British Summer time
// code from https://my-small-projects.blogspot.com/2015/05/arduino-checking-for-british-summer-time.html

boolean IsBST()
{
    int imonth = month();
    int iday = day();
    int hr = hour();
    
    //January, february, and november are out.
    if (imonth < 3 || imonth > 10) { return false; }
    //April to September are in
    if (imonth > 3 && imonth < 10) { return true; }

    // find last sun in mar and oct - quickest way I've found to do it
    // last sunday of march
    int lastMarSunday =  (31 - (5* year() /4 + 4) % 7);
    //last sunday of october
    int lastOctSunday = (31 - (5 * year() /4 + 1) % 7);
        
    //In march, we are BST if is the last sunday in the month
    if (imonth == 3) { 
      
      if( iday > lastMarSunday)
        return true;
      if( iday < lastMarSunday)
        return false;
      
      if (hr < 1)
        return false;
              
      return true; 
  
    }
    //In October we must be before the last sunday to be bst.
    //That means the previous sunday must be before the 1st.
    if (imonth == 10) { 

      if( iday < lastOctSunday)
        return true;
      if( iday > lastOctSunday)
        return false;  
      
      if (hr >= 1)
        return false;
        
      return true;  
    }

}  // IsBST


//-----------------------------------------------------------------------------
//        send an NTP request to the time server at the given address
//-----------------------------------------------------------------------------

void sendNTPpacket(const char* address) {
  
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum, or type of clock
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now you can send a packet requesting a timestamp:
  // Note that Udp.begin will request automatic translation (via a DNS server) from a
  // name (eg pool.ntp.org) to an IP address. Never use a specific IP address yourself,
  // let the DNS give back a random server IP address
  NTPUdp.beginPacket(address, 123); //NTP requests are to port 123

  // Get the data back
  NTPUdp.write(packetBuffer, NTP_PACKET_SIZE);

  // All done, the underlying buffer is now updated
  NTPUdp.endPacket();
  
}  // sendNTPpacket


//-----------------------------------------------------------------------------
//                contact the NTP pool and retrieve the time
//-----------------------------------------------------------------------------

time_t getNTPTime() {

  // Send a UDP packet to the NTP pool address
  if (serialDebug) {
    Serial.print("\nSending NTP packet to ");
    Serial.print(timeServer);
    Serial.print(": ");
  }
  sendNTPpacket(timeServer);

  // Wait to see if a reply is available - timeout after X seconds. At least
  // this way we exit the 'delay' as soon as we have a UDP packet to process
  #define UDPtimeoutSecs 3
  int timeOutCnt = 0;
  while (NTPUdp.parsePacket() == 0 && ++timeOutCnt < (UDPtimeoutSecs * 10)){
    delay(100);
  }

  // Is there UDP data present to be processed? Sneak a peek!
  if (NTPUdp.peek() != -1) {
    // We've received a packet, read the data from it
    NTPUdp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // The time-stamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900)
    unsigned long secsSince1900 = highWord << 16 | lowWord;     // shift highword 16 binary places to the left then combine with lowword
    if (serialDebug) {
      Serial.print("Seconds since Jan 1 1900 = ");
      Serial.println(secsSince1900);
    }

    // now convert NTP time into everyday time:

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     // UL denotes it is 'unsigned long' 

    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    // Reset the interval to get the time from NTP server in case we previously changed it
    setSyncInterval(_resyncSeconds);

    return epoch;
  }

  // Failed to get an NTP/UDP response
    if (serialDebug) Serial.println("No NTP response received");
    setSyncInterval(_resyncErrorSeconds);       // try more frequently until a response is received

    return 0;
    
}  // getNTPTime


// ----------------------------------------------------------------
//                        request a web page
// ----------------------------------------------------------------
// parameters: ip address, page to request, port to use (usually 80), maximum chars to receive, ignore all in reply before this text 
//     return: web page reply as a string
//      Usage: requestWebPage("192.168.1.166", "/log", 80, 600, "");

String requestWebPage(String ip, String page, int port, int maxChars, String cuttoffText = ""){

  int maxWaitTime = 3000;                 // max time to wait for reply (ms)

  char received[maxChars + 1];            // temp store for incoming character data
  int received_counter = 0;               // counter of number of characters which have been received

  if (!page.startsWith("/")) page = "/" + page;     // make sure page begins with "/" 

  if (serialDebug) {
    Serial.print("requesting web page: ");
    Serial.print(ip);
    Serial.println(page);
  }
     
    WiFiClient client;

    // Connect to the site 
      if (!client.connect(ip.c_str() , port)) {                                      
        if (serialDebug) Serial.println("Web client connection failed");   
        return "web client connection failed";
      } 
      if (serialDebug) Serial.println("Connected to host - sending request...");
    
    // send request - A basic request looks something like: "GET /index.html HTTP/1.1\r\nHost: 192.168.0.4:8085\r\n\r\n"
      client.println("GET " + page + " HTTP/1.1 ");
      client.println("Host: " + ip );
      client.println("User-Agent: arduino-ethernet");
      client.println("Connection: close");
      client.println();    // needed to end HTTP header
  
      if (serialDebug) Serial.println("Request sent - waiting for reply...");
  
    // Wait for a response
      uint32_t ttimer = millis();
      while ( client.connected() && !client.available() && (uint32_t)(millis() - ttimer) < maxWaitTime ) {
        delay(10);
      }
      if ( ((uint32_t)(millis() - ttimer) > maxWaitTime ) && serialDebug) Serial.println("-Timed out");

    // read the response
      while ( client.connected() && client.available() && received_counter < maxChars ) {
        delay(4); 
        received[received_counter] = char(client.read());     // read one character
        received_counter+=1;
      }
      received[received_counter] = '\0';     // end of string marker
            
    if (serialDebug) {
      Serial.println("--------received web page-----------");
      Serial.println(received);
      Serial.println("------------------------------------");
      Serial.flush();     // wait for serial data to finish sending
    }
    
    client.stop();    // close connection
    if (serialDebug) Serial.println("Connection closed");

    // if cuttoffText was supplied then only return the text following this 
      if (cuttoffText != "") {
        char* locus = strstr(received,cuttoffText.c_str());    // locus = pointer to the found text
        if (locus) {                                           // if text was found
          if (serialDebug) Serial.println("The text '" + cuttoffText + "' was found in reply");
          return locus;                                        // return the reply text following 'cuttoffText'
        } else if (serialDebug) Serial.println("The text '" + cuttoffText + "' WAS NOT found in reply");
      }
    
  return received;        // return the full reply text
  
}  // requestWebPage


/*
    Idea for better code:
    
                                char espBuffer[1024] = {0};
                                int readCount = 0;
                                long startTime = millis();

                                while (millis() - startTime < 5000) { // Run for at least 5 seconds 
                                // Check to make sure we don't exceed espBuffer's boundaries
                                    if (ESPserial.available() > readCount + sizeof espBuffer - 1) break;
                                    readCount += ESPserial.readBytes(espBuffer + readCount, ESPserial.available());
                                }

*/

// --------------------------- E N D -----------------------------
