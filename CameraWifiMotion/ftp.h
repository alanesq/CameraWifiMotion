/**************************************************************************************************
 *  
 *                                         FTP images to server - 12Mar20 
 *                                         
 *                                         https://github.com/ldab/ESP32_FTPClient
 * 
 **************************************************************************************************/


#include <ESP32_FTPClient.h>

// Enter your FTP account details here  (NOTE: needs to have FTP_ENABLED set to 1 in the main settings) 
char ftp_server[] = "<ftp servers ip address>";
char ftp_user[]   = "<ftp user name>";
char ftp_pass[]   = "<ftp password>";

ESP32_FTPClient ftp (ftp_server,ftp_user,ftp_pass, 3000, 2);             // timeout and debbug mode on the last 2 arguments

// forward declarations
  void uploadImageByFTP(uint8_t*, size_t, String);

 
// ----------------------------------------------------------------
//                         upload image via ftp
// ----------------------------------------------------------------
// pass image frame buffer to upload the image to ftp server and file name to use

void uploadImageByFTP(uint8_t* buf, size_t len, String fName) {

  Serial.println("FTP image");
  
  ftp.OpenConnection();
  
  // // Make a new directory
  //  ftp.InitFile("Type A");
  //  ftp.MakeDir("my_new_dir");
  
  
  //Create a file and write the image data to it;
    ftp.ChangeWorkDir("/");
    ftp.InitFile("Type I");
    fName += ".jpg"; 
    ftp.NewFile( fName.c_str() );
    ftp.WriteData(buf, len);
    ftp.CloseFile();
  
  ftp.CloseConnection();

  // log_system_message("FTP");
}

  
// ---------------------------------------------- end ----------------------------------------------
