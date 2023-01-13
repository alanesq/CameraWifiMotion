/**************************************************************************************************
 *
 *                                    Send emails from ESP8266/ESP32
 *
 *    using ESP_Mail_Client library  -  https://github.com/mobizt/ESP-Mail-Client
 *
 *    part of the BasicWebserver sketch - https://github.com/alanesq/BasicWebserver
 *
 *
 *   This will fail if using Gmail unless your Google account has the following option set:
 *       Allow less secure apps: ON       see:  https://myaccount.google.com/lesssecureapps
 *       Also requires POP access to be enabled.
 *   GMX.COM emails work very well with no additional setup other than enable POP access.
 *
 *                                              20jan22
 *
 **************************************************************************************************

 Usage:

  In main code include:      #include "email.h"


  Using char arrays: // https://www.tutorialspoint.com/arduino/arduino_strings.htm


  Send a test email:
      _message[0]=0; _subject[0]=0;          // clear any existing text
      strcat(_subject,"test message");
      strcat(_message,"this is a test email from the esp");
      sendEmail(_emailReceiver, _subject, _message);

      Note: To also send an sms along with the email use:     sendSMSflag = 1;


 **************************************************************************************************/

//                                          S e t t i n g s

  const int EmailAttemptTime = 30;                          // how often to re-attempt failed email sends (seconds)

  const int MaxEmailAttempts = 5;                           // maximum email send attempts


// blank settings
  #define _emailReceiver "<email address>"                  // address to send emails to
  #define _smsReceiver "<email address>"                    // address to send text messages to
  #define _UserDomain "<domain to report from>"             // user domain to report in email
  #define _mailUser "<email address>"                       // address to send from
  #define _mailPassword "<email password>"                  // email password
  #define _SMTP "<smtp server>"                             // smtp server address
  #define _SMTP_Port 587                                    // port to use (gmail: Port for SSL: 465, Port for TLS/STARTTLS: 587)


const int maxMessageLength = 500;                             // maximum length of email message
const int maxSubjectLength = 150;                             // maximum length of email subject


//  ----------------------------------------------------------------------------------------


bool sendSMSflag = 0;                                       // if set then also send sms when any email is sent
bool emailToSend = 0;                                       // flag if there is an email waiting to be sent
uint32_t lastEmailAttempt = 0;                              // last time sending of an email was attempted
int emailAttemptCounter = 0;                                // counter for failed email attempts

#include <ESP_Mail_Client.h>

// stores for email messages
  char _message[maxMessageLength];
  char _subject[maxSubjectLength];
  char _recepient[80];

// forward declarations
    void smtpCallback(SMTP_Status status);
    bool sendEmail(char*, char* , char*);
    void EMAILloop();

/* The SMTP Session object used for Email sending */
  SMTPSession smtp;


// ----------------------------------------------------------------------------------------


// Called from LOOP to handle emails

void EMAILloop() {

  if (!emailToSend || emailAttemptCounter >= MaxEmailAttempts) return;

  if (lastEmailAttempt > 0 && (unsigned long)(millis() - lastEmailAttempt) < (EmailAttemptTime * 1000)) return;

  // try to send the email
    if (sendEmail(_recepient, _subject, _message)) {
      // email sent ok
        emailToSend = 0;                                  // clear flag that there is an email waiting to be sent
        sendSMSflag = 0;                                  // clear sms flag
        _recepient[0]=0; _message[0]=0; _subject[0]=0;    // clear all stored email text
        emailAttemptCounter = 0;                          // reset attempt counter
    } else {
      // email failed to send
        // log_system_message("Email send attempt failed, will retry in " + String(EmailAttemptTime) + " seconds");
        lastEmailAttempt = millis();                      // update time of last attempt
        emailAttemptCounter ++;                           // increment attempt counter
        if (emailAttemptCounter >= MaxEmailAttempts) {
          log_system_message("Error: Max email attempts exceded, email send has failed");
          emailToSend = 0;                                // clear flag that there is an email waiting to be sent
          sendSMSflag = 0;                                // clear sms flag
        }
    }

}  // EMAILloop


// ----------------------------------------------------------------------------------------


// Function send an email
//   see full example: https://github.com/mobizt/ESP-Mail-Client/blob/master/examples/Send_Text/Send_Text.ino

bool sendEmail(char* emailTo, char* emailSubject, char* emailBody) {

  if (serialDebug) Serial.println("----- sending an email -------");

  // enable debug info on serial port
    if (serialDebug) {
      smtp.debug(1);                       // turn debug reporting on
      smtp.callback(smtpCallback);         // Set the callback function to get the sending results
    }

  // Define the session config data which used to store the TCP session configuration
    ESP_Mail_Session session;

  // Set the session config
    session.server.host_name =  _SMTP;
    session.server.port = _SMTP_Port;
    session.login.email = _mailUser;
    session.login.password = _mailPassword;
    session.login.user_domain = _UserDomain;

  // Define the SMTP_Message class variable to handle to message being transported
    SMTP_Message message;
    // message.clear();

  // Set the message headers
    message.sender.name = _SenderName;
    message.sender.email = _mailUser;
    message.subject = emailSubject;
    message.addRecipient("receiver", emailTo);
    if (sendSMSflag) message.addRecipient("name2", _smsReceiver);
    // message.addCc("email3");
    // message.addBcc("email4");

  // Set the message content
    message.text.content = emailBody;

  // Misc settings
    message.text.charSet = "us-ascii";
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
    message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;
    // message.addHeader("Message-ID: <abcde.fghij@gmail.com>");    // custom message header

//  // Add attachment to the message
//    message.addAttachment(att);

  // Connect to server with the session config
    if (!smtp.connect(&session)) {
      log_system_message("Sending email '" + String(_subject) +"' failed, SMTP: " + smtp.errorReason());
      return 0;
    }

  // Start sending Email and close the session
    if (!MailClient.sendMail(&smtp, &message, true)) {
      log_system_message("Sending email '" + String(_subject) +"' failed, Send: " + smtp.errorReason());
      return 0;
    } else {
      log_system_message("Email '" + String(_subject) +"' sent ok");
      return 1;
    }

}


// ----------------------------------------------------------------------------------------


/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status) {

  if (!serialDebug) return;

  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    Serial.println("----------------");
    Serial.printf("Message sent success: %d\n", status.completedCount());
    Serial.printf("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      Serial.printf("Message No: %d\n", i + 1);
      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      Serial.printf("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\n", result.recipients);
      Serial.printf("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }

}

// --------------------------- E N D -----------------------------
