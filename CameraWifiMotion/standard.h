/**************************************************************************************************
 *
 *                       Modified version of my 'Standard' procedures  - 26Feb20
 *             
 *  
 **************************************************************************************************/


// forward declarations
  void log_system_message(String);
  String webheader(String);
  String webfooter();
  void handleLogpage();
  void handleNotFound();
  String requestpage();
  void handleReboot();
  void WIFIcheck();
  void UpdateBootlogSpiffs(String);             // in CameraWifiMotion.ino  
  

// ----------------------------------------------------------------
//                              -Startup
// ----------------------------------------------------------------
  
// html text colour codes  
  const String red = "<font color='#FF0000'>";           // red text
  const String green = "<font color='#006F00'>";         // green text
  const String blue = "<font color='#0000FF'>";          // blue text
  const String endcolour = "</font>";                    // end coloured text

String system_message[LogNumber + 1];                    // system log messages


// ----------------------------------------------------------------
//                      -log a system message  
// ----------------------------------------------------------------

void log_system_message(String smes) {

  //scroll old log entries up 
    for (int i=0; i < LogNumber; i++){
      system_message[i]=system_message[i+1];
    }

  // add the message
    system_message[LogNumber] = currentTime() + " - " + smes;
  
  // also send message to serial port
    Serial.println(system_message[LogNumber]);
}



// ----------------------------------------------------------------
//                         -header (html) 
// ----------------------------------------------------------------
// HTML at the top of each web page
//    additional style settings can be included

String webheader(String style = "") {

    String message = 
      "<!DOCTYPE html>\n"
      "<html>\n"
         "<head>\n"
           "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
           "<link rel=\'icon\' href=\'data:,\'>\n"
           "<title>" + stitle + "</title>\n"
           "<style>\n"                             /* Settings here for the top of screen menu appearance */
             "ul {list-style-type: none; margin: 0; padding: 0; overflow: hidden; background-color: rgb(128, 64, 0);}\n"
             "li {float: left;}\n"
             "li a {display: inline-block; color: white; text-align: center; padding: 30px 20px; text-decoration: none;}\n"
             "li a:hover { background-color: rgb(100, 0, 0);}\n" 
             + style + "\n"
           "</style>\n"
         "</head>\n"
         "<body style='color: rgb(0, 0, 0); background-color: yellow; text-align: center;'>\n"
           "<ul>\n"                  
             "<li><a href='/'>Home</a></li>\n"                       /* home menu button */
             "<li><a href='/log'>Log</a></li>\n"                     /* log menu button */
             // "<li><a href='/bootlog'>BootLog</a></li>\n"          /* boot log menu button */
             "<li><a href='/live'>Capture Image</a></li>\n"             /* live menu button */
             "<li><a href='/images'>Stored Images</a></li>\n"        /* last menu button */
             "<li><a href='/imagedata'>Raw Data</a></li>\n"          /* raw data menu button */
             "<h1>" + red + stitle + endcolour + "</h1>\n"           /* display the project title in red */
           "</ul>\n";

    return message;
    
}


// ----------------------------------------------------------------
//                             -footer (html)
// ----------------------------------------------------------------

// HTML at the end of each web page


String webfooter() {

     // NTP server link status
            String NTPtext = "NTP Link "; 
            if (NTPok == 1) NTPtext += "OK";
            else NTPtext += "Down";

      String message = 
      
             "<br>\n" 
             
             /* Status display at bottom of screen */
             "<div style='text-align: center;background-color:rgb(128, 64, 0)'>\n" 
                "<small>" + red + 
                    "<a href='https://github.com/alanesq/CameraWifiMotion'>" + stitle + "</a> " + sversion + 
                    " | Memory:" + String(ESP.getFreeHeap() / 1000) + "K" + 
                    " | Wifi: " + String(WiFi.RSSI()) + "dBm" 
                    " | " + NTPtext + 
                    " | Spiffs:" + String( (SPIFFS.totalBytes() - SPIFFS.usedBytes()) / 1000 ) + "K" +
                    // " | MAC: " + String( WiFi.macAddress() )  + 
                endcolour + "</small>\n" 
             "</div>\n" 
               
             /* end of HTML */  
             "</body>\n" 
             "</html>\n";

      return message;
}



// ----------------------------------------------------------------
//   -log web page requested    i.e. http://x.x.x.x/log
// ----------------------------------------------------------------


void handleLogpage() {

   log_system_message("log webpage requested");     

    // build the html for /log page

    String message = webheader();      // add the standard header

      message += "<P>\n";                // start of section
  
      message += "<br>SYSTEM LOG<br><br>\n";
  
      // list all system messages
      for (int i=LogNumber; i != 0; i--){
        if (i == LogNumber) message += red;           // most recent entry
        message += system_message[i];
        if (i == LogNumber) message += endcolour;
        message += "<BR>\n"; 
      }
  
      // message += "<a href='/'>BACK TO MAIN PAGE</a>\n";       // link back to root page
  
      message += "<br>" + webfooter();     // add standard footer html
    

    server.send(200, "text/html", message);    // send the web page

}


// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------

void handleNotFound() {
  
  log_system_message("Invalid web page requested");      
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  message = "";      // clear variable
  
}



// ----------------------------------------------------------------
//        -request a web page and return reply as a string
// ----------------------------------------------------------------
//
//     parameters = ip address, page to request, port to use (usually 80)     e.g.   "alanesq.com","/index.htm",80


String requestpage(const char* ip, String page, int port){

  Serial.print("requesting web page: ");
  Serial.println(ip + page);
  //log_system_message("requesting web page");      

  // Connect to the site 
    WiFiClient client;
    if (!client.connect(ip, port)) {
      Serial.println("Connection failed :-(");
      log_system_message("Error: Web connection failed");      
      return "connection failed";
    }  
    Serial.println("Connected to host - sending request...");


    // request the page
    client.print(String("GET " + page + " HTTP/1.1\r\n") +
                 "Host: " + ip + "\r\n" + 
                 "Connection: close\r\n\r\n");
  
    Serial.println("Request sent - waiting for reply...");
  
    //Wait up to 5 seconds for server to respond then read response
    int i = 0;
    while ((!client.available()) && (i < 500)) {
      delay(10);
      i++;
    }
    
    String wpage="";    // reply stored here
    
    // Read the entire response up to 200 characters
    while( (client.available()) && (wpage.length() <= 200) ) {
      wpage += client.readStringUntil('\r');     
    }
    Serial.println("-----received web page--------");
    Serial.println(wpage);
    Serial.println("------------------------------");

    client.stop();    // close connection
    Serial.println("Connection closed.");

  return wpage;

}


// ----------------------------------------------------------------
//   -reboot web page requested        i.e. http://x.x.x.x/reboot  
// ----------------------------------------------------------------
//
//     note: this fails if the esp has just been reflashed and not restarted


void handleReboot(){

      String message = "Rebooting....";
      server.send(404, "text/plain", message);   // send reply as plain text

      // rebooting
        UpdateBootlogSpiffs("Rebooting - URL request");     // update bootlog
        delay(500);          // give time to send the above html
        ESP.restart();   
        delay(5000);         // restart fails without this line

}


// --------------------------------------------------------------------------------------
//                                -wifi connection check
// --------------------------------------------------------------------------------------


void WIFIcheck() {
  
    if (WiFi.status() != WL_CONNECTED) {
      if ( wifiok == 1) {
        log_system_message("Wifi connection lost");          // log system message if wifi was ok but now down
        wifiok = 0;                                          // flag problem with wifi
      }
    } else { 
      // wifi is ok
      if ( wifiok == 0) {
        log_system_message("Wifi connection is back");       // log system message if wifi was down but now back
        wifiok = 1;                                          // flag wifi is now ok
      }
    }

}


// --------------------------- E N D -----------------------------
