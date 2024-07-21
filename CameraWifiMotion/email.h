/**************************************************************************************************
 *
 *                             Send emails from ESP32 v3.0  - 20Jul24
 *
 *    using ESP Mail Client@2.7.2 library  -  https://github.com/mobizt/ESP-Mail-Client
 *
 *    Allows sending of emails and also commands to the sketch can be received via emails
 *
 *   This will fail if using Gmail unless your Google account has the following option set:
 *       Allow less secure apps: ON       see:  https://myaccount.google.com/lesssecureapps
 *       Also requires POP access to be enabled.
 *   GMX.COM emails work very well with no additional setup other than enable POP access.
 *                                        
 *
 **************************************************************************************************

 Usage:

    In main code:    #include "email.h"   plus it requires the setup and loop procedures to be called

    Using char arrays: // https://www.tutorialspoint.com/arduino/arduino_strings.htm

    Send a test email:
      _recepient[0]='\0'; _message[0]='\0'; _subject[0]='\0';      // clear any existing text
      strcat(_recepient, _emailReceiver);                          // email address to send it to
      strcat(_subject,stitle);
      strcat(_subject,"test");
      strcat(_message,"test email");
      emailToSend=1; lastEmailAttempt=0; emailAttemptCounter=0; sendSMSflag=0;   // set flags that there is an email to be sent


        Notes: To also send an sms along with the email set 'sendSMSflag' to 1.
               Sending emails can take up to 1min if the system time is not set.
               Receiving emails can stop the esp32 responding whilst it is being read.


              To send attachments using Spiffs the ESP_Mail.FS.h file in the library needs to be modified
              delete block of lines around 73-92 and replace with
                  #if defined(ESP32)
                  #include <SPIFFS.h>
                  #endif
                  #define ESP_MAIL_DEFAULT_FLASH_FS SPIFFS       
              see: https://github.com/mobizt/ESP-Mail-Client/blob/master/src/ESP_Mail_FS.h            


 **************************************************************************************************/

#include <ESP_Mail_Client.h>

// define the email commands structure
  typedef void (*myFuncPtr)(void);         // typedef for the function call
  struct emailCommands {
    String command;                        // i.e. email subject which will trigger it
    bool authentication;                   // flag if authentication is required to run this command
    myFuncPtr callbackFunction;            // procedure called when command is triggered
  };
  void myCallbackFunction(void);           // prototype function

// Password (gives access to protected commands) - used with imap
  unsigned long authenticationPasswordTime = 0;               // time password was issued
  String authenticationPassword = "";                         // the random generated password
  
// stores for sending emails
  const int maxMessageLength = 1024;                          // maximum length of email message
  const int maxSubjectLength = 150;                           // maximum length of email subject
  char _message[maxMessageLength];
  char _subject[maxSubjectLength];
  char _recepient[80];
  bool sendSMSflag = 0;                                       // if set then also send sms when any email is sent
  bool emailToSend = 0;                                       // flag if there is an email waiting to be sent
  uint32_t lastEmailAttempt = 0;                              // last time sending of an email was attempted  
  int emailAttemptCounter = 0;                                // counter for failed email attempts
  
  
// forward declarations
  void setupIMAP();
  bool setupSMTP();
  bool sendAnEmail(const char *sEmail, const char *sSubject, const char *sMessage);
  void printPollingStatus(IMAPSession &imap);         /* Print the selected folder update info */
  void imapCallback(IMAP_Status status);              /* Callback function to get the Email reading status */
  void replySMTPCallback(SMTP_Status status);         /* Callback function to get the Email sending status */
  void printMessages(std::vector<IMAP_MSG_Item> &msgItems, bool headerOnly);
  void pwdRequested(void);
  void codeRedReceived(void);
  void setPswdForAll(bool newSetting);


/* ------------------------------------------------------------------------------------------- */
/*                                      S E T T I N G S                                        */

const int EmailAttemptTime = 30;                          // how often to re-attempt failed email sends (seconds)

const int MaxEmailAttempts = 3;                           // maximum email send attempts

const unsigned long authenticationPasswordTimeout = 120;  // time password is valid for (seconds)

#define _emailReceiver "<email address>"                  // an address to send emails to (Alan)

//#define ENABLE_EMAIL_RECEIVE 1                          // if email command receiving is used in this sketch (IMAP)
const String commanderEmail = "<email address>";          // email address to receive commands from (not working at present)

#define EMAIL_ADDRESS "<email address>"               // email address to send/receive emails with
#define EMAIL_PASSWORD "<password>"                     // email password
//#define EMAIL_FROM_NAME "BasicWebServer"                // who the emailes report to be from

// IMAP - incoming emails
  #define IMAP_HOST "<host>"
  #define IMAP_PORT 993

// SMTP - sending emails
  #define SMTP_HOST "<host>"
  #define SMTP_PORT 587


/*                           Procedures called when command received                           */
/*                Note: ENABLE_EMAIL_RECEIVE must be set for this to function                  */

  // 'test' 
  void testReceived(void) {
    // send a test email (quick method, single attempt only)
      char subject[]="test", message[]="test email has been received";
      sendAnEmail(_emailReceiver, subject, message);       
  }



/*                                   The email commands                                        */

  emailCommands emailCommandsInst[] {


    // command subject to trigger, authentication required, the procedure called when the command is received

            { "test", 0, &testReceived }                       // test

  };
int commandCount = sizeof(emailCommandsInst) / sizeof(*emailCommandsInst);     // total number of commands available


/*                                 E N D   O F   S E T T I N G S                               */
/* ------------------------------------------------------------------------------------------- */  
 

// email stuff 
  String lastReceivedSubject = "";                // the last received email subject received

  // IMAP
    #if ENABLE_EMAIL_RECEIVE
      IMAPSession imap;                           /* Declare the global used IMAPSession object for IMAP transport */
      Session_Config imap_config;                 /* Declare the global used Session_Config for user defined IMAP session credentials */
      IMAP_Data imap_data;                        /* Define the IMAP_Data object used for user defined IMAP operating options * and contains the IMAP operating result */
    #endif
    bool imapSetupOk = false;
    
  // SMTP
    SMTPSession smtp;                             /* Declare the global used SMTPSession object for SMTP transport */
    Session_Config smtp_config;                   /* Declare the global used Session_Config for user defined SMTP session credentials */


// ---------------------------------------------------------------
//                             -setup
// ---------------------------------------------------------------
// called from main SETUP

void EMAILsetup() {
    MailClient.networkReconnect(true);            //  Set the email network reconnection option 
    
    // IMAP stuff (receiving emails)
    #if ENABLE_EMAIL_RECEIVE
        imap_data.download.header = true;
        imap_data.download.text = false;          // requires somewhere to store it (sd card, fat etc.)
        imap_data.download.attachment = false;
        imap_data.download.inlineImg = false;        
        
      // keep connection alive
        imap.keepAlive(5, 5, 1);      // see: https://github.com/mobizt/ESP-Mail-Client#using-tcp-session-keepalive-in-esp8266-and-esp32  
        if (serialDebug) Serial.println("imap keep alive status is: " + String(imap.isKeepAlive()) );         
    
      if (serialDebug) Serial.println("Setup and connect to IMAP server... ");
      setupIMAP();
      if (!imapSetupOk) {
          delay(500);                           // wait then try again
          setupIMAP();
          if (!imapSetupOk) log_system_message("ERROR: IMAP setup failed");
        }

      // set password to a random number to prevent it being triggered accidentally
        auto pwd = random(8999) + 1000;                      // generate a random number 
        authenticationPassword = String(pwd);        
    #endif
}


// ---------------------------------------------------------------
//                            -loop
// ---------------------------------------------------------------
// called from main LOOP

void EMAILloop() {

  // receiving emails

      #if ENABLE_EMAIL_RECEIVE
        if (imap.listen()) {    
          if (imap.folderChanged()) printPollingStatus(imap);      // if mailbox has changed
        }
      #endif


  // sending emails

    if (!emailToSend || emailAttemptCounter >= MaxEmailAttempts) return;     // no email to send 

    // if time to attempt sending an email
      if ( lastEmailAttempt==0 || (unsigned long)(millis() - lastEmailAttempt) > (EmailAttemptTime * 1000) ) {
        sendAnEmail(_recepient, _subject, _message);    // attempt to send an email
        lastEmailAttempt = millis();                    // set last time attempted timer to now
        emailAttemptCounter++;                          // increment attempt counter
        
        // if limit of tries reached
          if (emailAttemptCounter >= MaxEmailAttempts) {
            log_system_message("Error: Max email attempts exceded, email send has failed");
            emailToSend = 0;                                // clear flag that there is an email waiting to be sent
            sendSMSflag = 0;                                // clear sms send flag
            lastEmailAttempt = 0;                           // reset timer
          }    
      }
}

// ---------------------------------------------------------------
//               Configure IMAP - for email receiving
// --------------------------------------------------------------- 

#if ENABLE_EMAIL_RECEIVE
void setupIMAP() {
    imap.debug(1);                   // enable debug info on serial

    #if (defined ESP32)  
      esp_task_wdt_reset();          // reset watchdog timer in case it takes a while
    #endif        

    imap.callback(imapCallback);     // Set the callback function to get the reading results 

    /* Set the imap app config */
    imap_config.server.host_name = IMAP_HOST;
    imap_config.server.port = IMAP_PORT;
    imap_config.login.email = EMAIL_ADDRESS;
    imap_config.login.password = EMAIL_PASSWORD;

    // set system time  (receiving emails can take much longer if this is not set)
      time_t t=now();               // get current time
      imap.setSystemTime(t, 0);     // linux time, offset      

    // Connect to the server 
      if (!imap.connect(&imap_config, &imap_data)) {
        log_system_message("IMAP client, ERROR: failed to connect");
        return;
      }
      if (imap.isAuthenticated()) log_system_message("IMAP client, successfully logged in.");
      else log_system_message("IMAP client, connected with no Auth.");

    // Open or select the mailbox folder to read or search the message 
      if (!imap.selectFolder(F("INBOX"))) {
        log_system_message("IMAP client, ERROR: failed to select INBOX");
        return;
      }

    imapSetupOk = true;
}
#endif


// ---------------------------------------------------------------
//               Configure SMTP - for email sending
// --------------------------------------------------------------- 

bool setupSMTP() {
  
    smtp.debug(1);                       // enable debug info on serial
    
    #if (defined ESP32)  
      esp_task_wdt_reset();              // reset watchdog timer  
    #endif    

    smtp.callback(replySMTPCallback);    // Set the callback function to get the sending results 

    /* Set the session config */
      smtp_config.server.host_name = SMTP_HOST;
      smtp_config.server.port = SMTP_PORT;
      smtp_config.login.email = EMAIL_ADDRESS;
      smtp_config.login.password = EMAIL_PASSWORD;
      smtp_config.login.user_domain = "";

    // set system time  (sending can take much longer if this is not set)
      time_t t=now();               // get current time
      smtp.setSystemTime(t, 0);     // linux time, offset        

    // Connect to the server 
      if (!smtp.connect(&smtp_config))
      {
          if (serialDebug) MailClient.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
          log_system_message("Error connecting to SMTP server");
          return false;
      }
      if (smtp.isAuthenticated()) log_system_message("SMTP client has successfully logged in");
      else log_system_message("SMTP client has connected with no Authority");

    return true;
}


// ---------------------------------------------------------------
//                          -send an email
// --------------------------------------------------------------- 

bool sendAnEmail(const char *sEmail, const char *sSubject, const char *sMessage) {

    if (!emailSendingEnabled) {
      log_system_message("Email blocked as sending is disabled");
      return 0;
    }  
  
    log_system_message("Email '" + String(sSubject) + "' send requested");
    
    if (serialDebug) Serial.println("----- sending an email -------");

    if (!setupSMTP()) {
      log_system_message("Email '" + String(sSubject) + "': send failed as unable to log in to smtp server");
      return 0;
    }

    #if (defined ESP32)  
      esp_task_wdt_reset();          // reset watchdog timer in case it takes a long time to send email
    #endif    

    /* Declare the message class */
    SMTP_Message message;
    message.enable.chunking = true;

    /* Set the message headers */
    message.sender.name = EMAIL_FROM_NAME;
    message.sender.email = EMAIL_ADDRESS;
    message.subject = sSubject;
    message.addRecipient(F("Me"), sEmail);

    // message - html
      String htmlMsg = "<span style=\"color:#ff0000;\">" + String(sMessage) + "</span><br/><br/><img src=\"orange.png\" width=\"100\" height=\"100\"> ";
      message.html.content = htmlMsg.c_str();
      message.html.charSet = F("utf-8");
      message.html.transfer_encoding = Content_Transfer_Encoding::enc_qp;

    // message - plain text
      message.text.content = sMessage;
      message.text.charSet = F("utf-8");
      message.text.transfer_encoding = Content_Transfer_Encoding::enc_base64;    

    // attach an image file (attachment) 
    // Note: library has to be modified to use spiffs (see comments at top of sketch for more info.)
      SMTP_Attachment att;
      message.enable.chunking = true;   // Enable the chunked data transfer with pipelining for large message if server supported
      String tString = "";
      if (SpiffsFileCounter > 0) tString = String(SpiffsFileCounter) + "s.jpg";  // image file name (in root directory)
      if (tString != "") {                                                       // if there is an image to attach   
        if (SPIFFS.exists("/" + tString)) {
          att.descr.filename = tString.c_str();
          att.descr.mime = "image/jpeg"; 
          tString = "/" + tString;
          att.file.path = tString.c_str();
          att.file.storage_type = esp_mail_file_storage_type_flash;
          att.descr.transfer_encoding = Content_Transfer_Encoding::enc_base64;
          message.addAttachment(att);
          log_system_message("File '" + tString + "' attached to email");
        } else {
          log_system_message("Error: File '" + tString + "' not found in SPIFFS");
        }
      }
      
    /* Start sending Email and close the session */
    if (!MailClient.sendMail(&smtp, &message)) {
        if (serialDebug) MailClient.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
        log_system_message("Email '" + String(sSubject) + "': send failed");
        return 0;
    } 

    return 1;
}


// ---------------------------------------------------------------
//                called when email inbox has changed?
// ---------------------------------------------------------------

#if ENABLE_EMAIL_RECEIVE
void printPollingStatus(IMAPSession &imap) {

    /* Declare the selected folder info class to get the info of selected mailbox folder */
    SelectedFolderInfo sFolder = imap.selectedFolder();

    #if (defined ESP32)  
      esp_task_wdt_reset();          // reset watchdog timer  
    #endif       

    if (sFolder.pollingStatus().type == imap_polling_status_type_new_message)
    {
        /* Show the mailbox info */
        if (serialDebug) MailClient.printf("\nMailbox status changed\n----------------------\nTotal Messages: %d\n", sFolder.msgCount());
        if (serialDebug) MailClient.printf("New message %d, has been addedd, reading message...\n", (int)sFolder.pollingStatus().messageNum);
        log_system_message("A new email is in the in-box");

        // we need to stop polling before do anything
        imap.stopListen();

        // Get the UID of new message and fetch
        imap_data.fetch.uid = imap.getUID(sFolder.pollingStatus().messageNum);

        // When message was fetched or read, the /Seen flag will not set or message remained in unseen or unread status,
        // as this is the purpose of library (not UI application), user can set the message status as read by set \Seen flag
        // to message, see the Set_Flags.ino example.
        MailClient.readMail(&imap, false);
    }
}


// ---------------------------------------------------------------
//           Callback function when email is received
// --------------------------------------------------------------- 

void imapCallback(IMAP_Status status) {
  
    bool passwordReceived = 0;       // flag set if subject contains live password
    if (serialDebug) Serial.println(status.info());

    #if (defined ESP32)  
      esp_task_wdt_reset();          // reset watchdog timer  
    #endif       

    /* Show the result when reading finished */
    if (status.success())
    {
        /* Get the message list from the message list data */
        IMAP_MSG_List msgList = imap.data();
        IMAP_MSG_Item msg = imap.data().msgItems[0];               
        // Note: msg.from and msg.sender are blank - I suspect you have to use external storage to be able to get the message text etc.?
        printMessages(msgList.msgItems, imap.headerOnly());         // display all info about the received email

        // check if live password is contained within subject
          if (strstr(msg.subject, authenticationPassword.c_str()) != nullptr) {     // if password found
            if (authenticationPasswordTime != 0 && millis() - authenticationPasswordTime < (authenticationPasswordTimeout * 1000) ) {
              passwordReceived = 1;                                                 // flag password has been received
            } else {
              authenticationPasswordTime = 0;                                       // password not set or expired
              authenticationPassword = "";
            }
          }

        // go through all available email commands searching for a match
          for (int i=0; i < commandCount; i++) {
            if (strstr(msg.subject, emailCommandsInst[i].command.c_str()) != nullptr) {   // if command is contained in the email subject
              lastReceivedSubject = String(msg.subject);                                  // store the full subject line in global variable
              if (!emailCommandsInst[i].authentication || passwordReceived) {             // if password not required for this command or it was supplied
                log_system_message("Command '" + emailCommandsInst[i].command + "' has been received");
                emailCommandsInst[i].callbackFunction();                             // call the associated procedure                
              } else {
                log_system_message("Command '" + emailCommandsInst[i].command + "' rejected as it requires password");
              }
            }
          }

        /* Clear all stored data in IMAPSession object */
        imap.empty();
    }
}
#endif


// ---------------------------------------------------------------
//         Callback function when email has been sent
// --------------------------------------------------------------- 

void replySMTPCallback(SMTP_Status status) {

    #if (defined ESP32)  
      esp_task_wdt_reset();          // reset watchdog timer  
    #endif     

  // if email has sent ok
    if (status.success()) {
      log_system_message("Email has been sent");
      emailToSend = 0;                                  // clear flag that there is an email waiting to be sent
      sendSMSflag = 0;                                  // clear sms flag
      lastEmailAttempt = 0;                             // reset timer
      _recepient[0]=0; _message[0]=0; _subject[0]=0;    // clear all stored email text
      emailAttemptCounter = 0;                          // reset attempt counter
      smtp.sendingResult.clear();                       // clear sending results log
    } else {
      //log_system_message("Sending email: status: '" + String(status.info()) + "'");
    }

    if (!serialDebug) return;

    /* Print the current status */
    if (serialDebug) Serial.println(status.info());

    /* Print the sending result */
    if (status.success())
    {
        if (serialDebug) {
          Serial.println("----------------");
          MailClient.printf("Message sent success: %d\n", status.completedCount());
          MailClient.printf("Message sent failed: %d\n", status.failedCount());
          Serial.println("----------------\n");
        }
        struct tm dt;

        for (size_t i = 0; i < smtp.sendingResult.size(); i++)
        {
            /* Get the result item */
            SMTP_Result result = smtp.sendingResult.getItem(i);
            if (serialDebug) {
              MailClient.printf("Message No: %d\n", i + 1);
              MailClient.printf("Status: %s\n", result.completed ? "success" : "failed");
              MailClient.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
              MailClient.printf("Recipient: %s\n", result.recipients.c_str());
              MailClient.printf("Subject: %s\n", result.subject.c_str());
            }
        }
        if (serialDebug) Serial.println("----------------\n");

        smtp.sendingResult.clear();
    }
}


// ---------------------------------------------------------------
//          Print all details about incoming email 
// --------------------------------------------------------------- 

void printMessages(std::vector<IMAP_MSG_Item> &msgItems, bool headerOnly) 
{
    if (!serialDebug) return;
    Serial.println("\n------------- email content -------------");

    /** In devices other than ESP8266 and ESP32, if SD card was chosen as filestorage and
     * the standard SD.h library included in ESP_Mail_FS.h, files will be renamed due to long filename
     * (> 13 characters) is not support in the SD.h library.
     * To show how its original file name, use imap.fileList().
     */
    // Serial.println(imap.fileList());

    for (size_t i = 0; i < msgItems.size(); i++) {

        /* Iterate to get each message data through the message item data */
        IMAP_MSG_Item msg = msgItems[i];

        Serial.println("****************************");
        MailClient.printf("Number: %d\n", msg.msgNo);
        MailClient.printf("UID: %d\n", msg.UID);

        // The attachment status in search may be true in case the "multipart/mixed"
        // content type header was set with no real attachtment included.
        MailClient.printf("Attachment: %s\n", msg.hasAttachment ? "yes" : "no");

        MailClient.printf("Messsage-ID: %s\n", msg.ID);

        if (strlen(msg.flags))
            MailClient.printf("Flags: %s\n", msg.flags);
        if (strlen(msg.acceptLang))
            MailClient.printf("Accept Language: %s\n", msg.acceptLang);
        if (strlen(msg.contentLang))
            MailClient.printf("Content Language: %s\n", msg.contentLang);
        if (strlen(msg.from))
            MailClient.printf("From: %s\n", msg.from);
        if (strlen(msg.sender))
            MailClient.printf("Sender: %s\n", msg.sender);
        if (strlen(msg.to))
            MailClient.printf("To: %s\n", msg.to);
        if (strlen(msg.cc))
            MailClient.printf("CC: %s\n", msg.cc);
        if (strlen(msg.bcc))
            MailClient.printf("BCC: %s\n", msg.bcc);
        if (strlen(msg.date))
        {
            MailClient.printf("Date: %s\n", msg.date);
            MailClient.printf("Timestamp: %d\n", (int)MailClient.Time.getTimestamp(msg.date));
        }
        if (strlen(msg.subject))
            MailClient.printf("Subject: %s\n", msg.subject);
        if (strlen(msg.reply_to))
            MailClient.printf("Reply-To: %s\n", msg.reply_to);
        if (strlen(msg.return_path))
            MailClient.printf("Return-Path: %s\n", msg.return_path);
        if (strlen(msg.in_reply_to))
            MailClient.printf("In-Reply-To: %s\n", msg.in_reply_to);
        if (strlen(msg.references))
            MailClient.printf("References: %s\n", msg.references);
        if (strlen(msg.comments))
            MailClient.printf("Comments: %s\n", msg.comments);
        if (strlen(msg.keywords))
            MailClient.printf("Keywords: %s\n", msg.keywords);

        /* If the result contains the message info (Fetch mode) */
        if (!headerOnly)
        {
            if (strlen(msg.text.content))
                MailClient.printf("Text Message: %s\n", msg.text.content);
            if (strlen(msg.text.charSet))
                MailClient.printf("Text Message Charset: %s\n", msg.text.charSet);
            if (strlen(msg.text.transfer_encoding))
                MailClient.printf("Text Message Transfer Encoding: %s\n", msg.text.transfer_encoding);
            if (strlen(msg.html.content))
                MailClient.printf("HTML Message: %s\n", msg.html.content);
            if (strlen(msg.html.charSet))
                MailClient.printf("HTML Message Charset: %s\n", msg.html.charSet);
            if (strlen(msg.html.transfer_encoding))
                MailClient.printf("HTML Message Transfer Encoding: %s\n\n", msg.html.transfer_encoding);

            if (msg.rfc822.size() > 0)
            {
                MailClient.printf("\r\nRFC822 Messages: %d message(s)\n****************************\n", msg.rfc822.size());
                printMessages(msg.rfc822, headerOnly);
            }
        }

        Serial.println("\n-----------------------------------------\n");
    }
}

// end
