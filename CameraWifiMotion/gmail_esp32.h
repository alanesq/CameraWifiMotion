/**************************************************************************************************
 * 
 *    Send emails from ESP32 via Gmail    -   15Mar20
 * 
 *    include in main sketch if sending emails is required with command     #include "gmail.h"
 *    
 *    uses: 'ESP32mail' library    
 * 
 *  This demo sketch will fail at the Gmail login unless your Google account has
 *  set the following option:     Allow less secure apps: ON       
 *                                see:  https://myaccount.google.com/lesssecureapps
 *
 *                                            Gmail - v2.0            
 *  
 **************************************************************************************************


 Usage:             // send email
                      String emessage = "<message here>";
                      byte q = sendEmail(emailReceiver,"<subject here>", emessage);    
                      if (q==0) log_system_message("email sent ok" );
                      else log_system_message("Error sending email code=" + String(q) ); 


*/
// -------------------------- S e t t i n g s ---------------------


  
// Blank Settings        -    *** ENTER YOUR DETAILS BELOW! ***
  const String emailReceiver = "<email to send to>";         // address to send emails  
  const String _mailUser = "<email to send from>";
  const String _mailPassword = "<email password>";
  

  const String _SMTP = "smtp.gmail.com";

  const String _SenderName = stitle;

  bool SendImage = 1;                                          // set to 1 if sending an image attachment with email
  

  
// ----------------------------------------------------------------


// forward declarations
  byte sendEmail();

  
// ----------------------------------------------------------------
//                              -Startup
// ----------------------------------------------------------------
  
#include "ESP32_MailClient.h"


//The Email Sending data object contains config and data to send
    SMTPData smtpData;


//preload Callback function 
    void sendCallback(SendStatus info);


    
// ----------------------------------------------------------------
//                     -Send an email via gmail
// ----------------------------------------------------------------

byte sendEmail(String emailTo, String emailSubject, String emailBody) {

  Serial.print("\nSending email: ");
  Serial.println(emailTo + "," + emailSubject + "," + emailBody);

  //Set the Email host, port, account and password
  smtpData.setLogin(_SMTP, 587, _mailUser, _mailPassword);

  //For library version 1.2.0 and later which STARTTLS protocol was supported,the STARTTLS will be 
  //enabled automatically when port 587 was used, or enable it manually using setSTARTTLS function.
  //smtpData.setSTARTTLS(true);

  //Set the sender name and Email
  smtpData.setSender(_SenderName, _mailUser);

  //Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
  smtpData.setPriority("High");

  //Set the subject
  smtpData.setSubject(emailSubject);

  //Set the message - normal text or html format
  smtpData.setMessage(emailBody, true);

  //Add recipients, can add more than one recipient
  smtpData.addRecipient(emailTo);

  //Add some custom header to message
  //See https://tools.ietf.org/html/rfc822
  //These header fields can be read from raw or source of message when it received)
    //smtpData.addCustomMessageHeader("Date: Sat, 10 Aug 2019 21:39:56 -0700 (PDT)");
  //Be careful when set Message-ID, it should be unique, otherwise message will not store
  //smtpData.addCustomMessageHeader("Message-ID: <abcde.fghij@gmail.com>");

  // set debug for smtp
    smtpData.setDebug(true);

  //Set the storage types to read the attach files (SD is default)
  smtpData.setFileStorageType(MailClientStorageType::SPIFFS);
  //smtpData.setFileStorageType(MailClientStorageType::SD);

  //Add attachments, can add the file or binary data from flash memory, file in SD card
  //Data from internal memory
  // smtpData.addAttachData("firebase_logo.png", "image/png", (uint8_t *)dummyImageData, sizeof dummyImageData);

  // attach image file to email
    if (SpiffsFileCounter > 0) {
      String TFileName = "/" + String(SpiffsFileCounter) + ".jpg";
      String SFileName = "/" + String(SpiffsFileCounter) + "s.jpg";
      if (SendImage) smtpData.addAttachFile(TFileName);
      if (SendImage) smtpData.addAttachFile(SFileName);
    }

  smtpData.setSendCallback(sendCallback);

  //Start sending Email, can be set callback function to track the status
  byte tcount = 5;    // number of attempts to try
  int tresult = 0;
  while (tcount > 0 && tresult == 0) {
  tresult = MailClient.sendMail(smtpData);
    tcount = tcount - 1;
    if (!tresult) {
      Serial.println("Error sending Email, " + MailClient.smtpErrorReason());
      delay(800);
    }
  }


  //Clear all data from Email object to free memory
  smtpData.empty();

  
  Serial.println("------ end of email send -----");
  return !tresult;    // 0 = ok

}




//Callback function to get the Email sending status
void sendCallback(SendStatus msg)
{
  //Print the current status
  Serial.println(msg.info());

  //Do something when complete
  if (msg.success())
  {
    Serial.println("----------------");
  }
}



// --------------------------- E N D -----------------------------
