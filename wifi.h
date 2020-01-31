/**************************************************************************************************
 *
 *                                    Wifi / NTP Stuff - 24Jan20
 *             
 *                    Set up wifi for either esp8266 or esp32 plus NTP (network time)
 *  
 **************************************************************************************************/


// forward declarations
  void startWifiManager();
  String currentTime();
  bool IsBST();
  void sendNTPpacket();
  time_t getNTPTime();



// ----------------------------------------------------------------
//                              -Startup
// ----------------------------------------------------------------

byte wifiok = 0;                    // flag if wifi is connected ok (1 = ok)
  
// Wifi libraries (for both ESP8266 and ESP32)
    #include <WiFiManager.h>            // see https://github.com/zhouhan0126/WIFIMANAGER-ESP32
    #if defined(ESP8266)                  // esp8266 section
      ESP8266WebServer server(ServerPort);
      const String ESPType = "ESP8266";
    #elif defined(ESP32)                  // esp32 section
      WebServer server(ServerPort); 
      const String ESPType = "ESP32";
    #else
        #error "Only works with ESP8266 or ESP32"
    #endif


// Time from NTP server
//      from https://raw.githubusercontent.com/RalphBacon/No-Real-Time-Clock-RTC-required---use-an-NTP/master
  #include <TimeLib.h>
  #include <WiFiUdp.h>                         // UDP library which is how we communicate with Time Server
  unsigned int localPort = 8888;               // Just an open port we can use for the UDP packets coming back in
  char timeServer[] = "uk.pool.ntp.org"; 
  const int NTP_PACKET_SIZE = 48;              // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[NTP_PACKET_SIZE];          // buffer to hold incoming and outgoing packets
  WiFiUDP NTPUdp;                              // A UDP instance to let us send and receive packets over UDP
  const int timeZone = 0;                      // timezone (0=GMT)
  String DoW[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  // How often to resync the time (under normal and error conditions)
    const int _resyncSeconds = 7200;           // 7200 = 2 hours
    const int _resyncErrorSeconds = 60;        // 60 = 1 min
  boolean NTPok = 0;                           // Flag if NTP is curently connecting ok



// ----------------------------------------------------------------
//                 -wifi initialise  (called from 'setup')
// ----------------------------------------------------------------

void startWifiManager() {

  // WiFiManager - connect to wifi with stored settings
    WiFiManager wifiManager;                                     // Local intialisation 
    wifiManager.setTimeout(120);                                 // sets timeout for configuration portal (in mins)
    Serial.println(F("Starting Wifi Manager"));
    if(!wifiManager.autoConnect( "ESPconfig" , "12345678" )) {   // Activate wifi - access point name and password are set here (password must be 8 characters for some reason)
      // Failed to connect to wifi and access point timed out
        Serial.println(F("failed to connect to wifi and access point timed out so rebooting to try again..."));
        Serial.flush();
        ESP.restart();                                             // reboot and try again
        delay(5000);                                               // restart will fail without this delay
    }

    // Now connected to Wifi network
    if (WiFi.status() == WL_CONNECTED) {
      wifiok = 1;     // flag wifi now ok  
      Serial.println(F("Wifi connected ok"));           
    } else {
      wifiok = 0;     // flag wifi down
      Serial.println(F("Problem connecting to Wifi"));   
    }

  // start NTP
    NTPUdp.begin(localPort);                  // What port will the UDP/NTP packet respond on?
    setSyncProvider(getNTPTime);              // What is the function that gets the time (in ms since 01/01/1900)?
    setSyncInterval(_resyncErrorSeconds);     // How often should we synchronise the time on this machine (in seconds) 
    
  // turn off sleep mode for esp8266
  #if defined(ESP8266)
    Serial.println(F("Setting sleep mode to off"));
    WiFi.setSleepMode(WIFI_NONE_SLEEP);     // Stops wifi turning off (if on causes wifi to drop out randomly)
  #endif
       
}




// ----------------------------------------------------------------
//               -Return current time and date as string
// ----------------------------------------------------------------

String currentTime(){

   time_t t=now();     // get current time 

   if (IsBST()) t+=3600;     // add one hour if it is Summer Time

   String ttime = String(hour(t)) + ":" ;                                               // hours
   if (minute(t) < 10) ttime += "0";                                                  // minutes
   ttime += String(minute(t)) + " ";
   ttime += DoW[weekday(t)-1] + " ";                                                  // day of week
   ttime += String(day(t)) + "/" + String(month(t)) + "/" + String(year(t)) + " ";    // date

   return ttime;

}



//-----------------------------------------------------------------------------
//                           -British Summer Time check
//-----------------------------------------------------------------------------

// returns true if it is British Summer time
//         code from https://my-small-projects.blogspot.com/2015/05/arduino-checking-for-british-summer-time.html

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

}



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
  
}



//-----------------------------------------------------------------------------
//                contact the NTP pool and retrieve the time
//-----------------------------------------------------------------------------
//
// code from https://github.com/RalphBacon/No-Real-Time-Clock-RTC-required---use-an-NTP

time_t getNTPTime() {

  // Send a UDP packet to the NTP pool address
  Serial.print(F("\nSending NTP packet to "));
  Serial.println(timeServer);
  sendNTPpacket(timeServer);

  // Wait to see if a reply is available - timeout after X seconds. At least
  // this way we exit the 'delay' as soon as we have a UDP packet to process
  #define UDPtimeoutSecs 3
  int timeOutCnt = 0;
  while (NTPUdp.parsePacket() == 0 && ++timeOutCnt < (UDPtimeoutSecs * 10)){
    delay(100);
    // yield();
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
    Serial.print("Seconds since Jan 1 1900 = ");
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //Serial.print("Unix time = ");

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     // UL denotes it is 'unsigned long' 

    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    // Reset the interval to get the time from NTP server in case we previously changed it
    setSyncInterval(_resyncSeconds);
    NTPok = 1;       // flag NTP is currently connecting ok

    return epoch;
  }

  // Failed to get an NTP/UDP response
    Serial.println(F("No NTP response"));
    setSyncInterval(_resyncErrorSeconds);       // try more frequently until a response is received
    NTPok = 0;                                  // flag NTP not currently connecting

    return 0;
  
}


// --------------------------- E N D -----------------------------
