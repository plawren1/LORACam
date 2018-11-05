// ArduCAM Mini demo (C)2017 Lee
// Web: http://www.ArduCAM.com
// This program is a demo of how to use most of the functions
// of the library with ArduCAM Mini camera, and can run on any Arduino platform.
// This demo was made for ArduCAM_Mini_5MP_Plus.
// It needs to be used in combination with PC software.
// It can test OV2640 functions
// This program requires the ArduCAM V4.0.0 (or later) library and ArduCAM_Mini_5MP_Plus
// and use Arduino IDE 1.6.8 compiler or above
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <SD.h>
#include "memorysaver.h"
#include <loraPortClass.h>
//This demo can only work on OV5642_MINI_5MP platform.
#if !(defined OV5642_MINI_5MP_PLUS)
  #error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif
#define BMPIMAGEOFFSET 66
const char bmp_header[BMPIMAGEOFFSET] PROGMEM =
{
  0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00,
  0x00, 0x00
};
// set pin 7 as the slave select for the digital pot:
const int CAM_CS = 6;
#define SD_CS 10
#define TESTVALUE 0x55
#define WHITE_LED 12
#define BLUE_LED 11
#define YELLOW_LED 9
#define RED_LED 13
#define GPIO_PWDN_MASK 2

uint8_t csCheck = 0;
bool is_header = false;
int mode = 0;
uint8_t start_capture = 0;
ArduCAM myCAM( OV5642, CAM_CS );
char buff[100];

bool yellowLed = true;
bool whiteLed = false;
bool blueLed = false;
bool sleeping = false;
bool redLed = false;
Sd2Card card;
File myFile;

//uint8_t read_fifo_burst(ArduCAM myCAM);
bool SDcardSetup();
bool CAMSetup();
bool capture ();
void readCard();
void getImage();
void createTestFile();

loraPortClass *port = NULL;

void setup() {
  // put your setup code here, to run once:
  #if defined(__SAM3X8E__)
    Serial.println(F("__SAM3X8E__!"));
    Wire1.begin();
    Serial.begin(115200);
  #else
    Serial.println(F("NOT __SAM3X8E__!"));
    Wire.begin();
    Serial.begin(115200);
  #endif
  
  pinMode(CAM_CS, OUTPUT);
  pinMode(WHITE_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  delay(5000);
  // initialize SPI:
  delay(5000);
  SPI.begin();

  if (!SDcardSetup())
  {
    Serial.println("Not ready - card setup failure!");
    digitalWrite(WHITE_LED, HIGH);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(BLUE_LED, HIGH);
  }

  delay(500);
  
  if (!CAMSetup())
  {
    Serial.println("Not ready - CAM setup failure!");
    digitalWrite(WHITE_LED, HIGH);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(BLUE_LED, HIGH);
  }
  else
  {
    Serial.println("Ready!");
    digitalWrite(WHITE_LED, HIGH);
  }

  port = new loraPortClass(portClass::slave, 10, 6);
  port->start();


  createTestFile();
}

void loop()
{
  // put your main code here, to run repeatedly:
  MSGS msg = port->checkMsgs();
  //MSGS msg = GETIMAGE;
  uint8_t send_value;
  
  switch(msg)
  {
    case CAMOFF:
      Serial.println("CAMOFF rxed");
      myCAM.set_bit(ARDUCHIP_GPIO,GPIO_PWDN_MASK); 
      Serial.println("CAM off!");
      sleeping = true;
      break;
    case CAMON:
      Serial.println("CAMON rxed");
      if (sleeping)
      {
        if (!CAMSetup())
        {
          delay(1000);
          if (!CAMSetup())
          {
            digitalWrite(WHITE_LED, LOW);
            digitalWrite(YELLOW_LED, LOW);
            digitalWrite(BLUE_LED, LOW);
          }
        }
        sleeping = false;
      }
      break;
    case CAPTURE:
      Serial.println("CAPTURE rxed");
      digitalWrite(YELLOW_LED, LOW);
      if (!sleeping)
      {
        if (capture())
        {
          port->sendCommand(CAPTURECOMPLETE);
          digitalWrite(BLUE_LED, LOW);
        }
        else
        {
          port->sendCommand(CAPTUREFAILED);
        }
      }
      else
      {
        Serial.println("Camera sleeping");
      }
      break;  
    case GETIMAGE:
      Serial.println("GETIMAGE rxed");
      getImage();
      break;
    case GETIMAGEACK:
    case IMAGEPARTACK:
    case IMAGEFINISHACK:
    case PING0:
    case NOMESSAGE:
      break;
    default:
      Serial.println("Lora Port error!");
      break;
  }

  int temp = 0;
  if (Serial.available())
  {
    temp = Serial.read();
    sprintf(buff, "rxed something %d", temp);
    switch (temp)
    {
      case 48://0
        if (sleeping)
        {
          if (!CAMSetup())
          {
            delay(1000);
            if (!CAMSetup())
            {
              digitalWrite(WHITE_LED, LOW);
              digitalWrite(YELLOW_LED, LOW);
              digitalWrite(BLUE_LED, LOW);
            }
          }
          sleeping = false;
        }
        break;
      case 49://1
        if (!sleeping)
        {
          capture();
        }
        else
        {
          Serial.println("Camera sleeping");
        }
        break;
      case 50://2
        myCAM.set_bit(ARDUCHIP_GPIO,GPIO_PWDN_MASK); 
        Serial.println("CAM off!");
        sleeping = true;
        break;
      case 51://3
        SDcardSetup();
        break;
      case 52://4
        CAMSetup();
        break;  
      case 53://5
        readCard();
        break;  
//      case 54://6
//        initLORA();
//        break;
      default:
        sprintf(buff, "rxed something invalid %d", temp);
        Serial.println(buff);
        break;
    }
  }

  // Pulse
  delay(500);
  if (redLed)
  {
    digitalWrite(RED_LED, LOW);
    redLed = false;
  }
  else
  {
    digitalWrite(RED_LED, HIGH);
    redLed = true;
  }
}

bool SDcardSetup()
{
    static bool bOnce = false;
    uint8_t failCounter = 0;
    SdVolume volume;
    SdFile root;

    if (bOnce)
      return true;
    
    //if (!card.init(SPI_HALF_SPEED, SD_CS))
    if (!bOnce && !card.init(SPI_FULL_SPEED, SD_CS))
    {
        Serial.println("initialization failed. Things to check:");
        Serial.println("* is a card inserted?");
        Serial.println("* is your wiring correct?");
        Serial.println("* did you change the chipSelect pin to match your shield or module?");

        if (failCounter++ >= 10)
        {
          return false;
        }
    } 
    else 
    {
        Serial.println("Wiring is correct and a card is present.");
        Serial.println(F("SD Card detected."));
        failCounter = 0;
    }

    digitalWrite(YELLOW_LED, LOW);
    delay(5000);

    while(!bOnce && !SD.begin(SD_CS))
    {
        Serial.println(F("SD Card Error"));delay(1000);
        if (yellowLed)
        {
            digitalWrite(YELLOW_LED, HIGH);
            yellowLed = LOW;
        }
        else
        {
            digitalWrite(YELLOW_LED, LOW);
            yellowLed = HIGH;
        }

        if (failCounter++ >= 10)
        {
          return false;
        }
    }
    
    bOnce = true;
    
    // print the type of card
    Serial.print("nCard type: ");
    switch(card.type()) 
    {
      case SD_CARD_TYPE_SD1:
        Serial.println("SD1");
        break;
      case SD_CARD_TYPE_SD2:
        Serial.println("SD2");
        break;
      case SD_CARD_TYPE_SDHC:
        Serial.println("SDHC");
        break;
      default:
        Serial.println("Unknown");
    }

    // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
    if (!volume.init(card)) 
    {
      Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
      return false;
    }
    
    // print the type and size of the first FAT-type volume
    uint32_t volumesize;
    Serial.print("\nVolume type is FAT");
    Serial.println(volume.fatType(), DEC);
    Serial.println();
  
    volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
    volumesize *= volume.clusterCount();       // we'll have a lot of clusters
    volumesize *= 512;                            // SD card blocks are always 512 bytes
    Serial.print("Volume size (bytes): ");
    Serial.println(volumesize);
    Serial.print("Volume size (Kbytes): ");
    volumesize /= 1024;
    Serial.println(volumesize);
    Serial.print("Volume size (Mbytes): ");
    volumesize /= 1024;
    Serial.println(volumesize);

    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    delay(2000);
    myFile = SD.open("TEST.TXT", FILE_WRITE);
  
    // if the file opened okay, write to it:
    if (myFile) 
    {
      Serial.print("Writing to test.txt...");
      myFile.println("testing 1, 2, 3.");
      // close the file:
      myFile.close();
      Serial.println("done.");
      digitalWrite(YELLOW_LED, HIGH);
    } else 
    {
      // if the file didn't open, print an error:
      Serial.println("error opening test.txt");
      digitalWrite(YELLOW_LED, LOW);
      return false;
    }
  
    // re-open the file for reading:
    myFile = SD.open("TEST.TXT");
    if (myFile) 
    {
      Serial.println("test.txt:");
  
      // read from the file until there's nothing else in it:
      while (myFile.available()) 
      {
        Serial.write(myFile.read());
      }
      // close the file:
      myFile.close();
      digitalWrite(YELLOW_LED, HIGH);
    } else 
    {
      // if the file didn't open, print an error:
      Serial.println("error opening test.txt");
      digitalWrite(YELLOW_LED, LOW);
      return false;
    }

    Serial.println("\nFiles found on the card (name, date and size in bytes): ");
    root.openRoot(volume);
 
    // list all files in the card with date and size
    root.ls(LS_R | LS_DATE | LS_SIZE);
    return true;
}

void readCard()
{
  SdVolume volume;
  SdFile root;

  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) 
  {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
  }

  Serial.println("\nFiles found on the card (name, date and size in bytes): ");
  root.openRoot(volume);

  // list all files in the card with date and size
  root.ls(LS_R | LS_DATE | LS_SIZE);
}

bool CAMSetup()
{
  uint8_t vid = 0; 
  uint8_t pid = 0;
  uint8_t temp = 0;
  uint8_t CAMTimeOut = 0;
  char buff[200];
  bool retValue = true;
  Serial.println(F("ACK CMD ArduCAM Start!"));
  
  while(1)
  {
      //Check if the ArduCAM SPI bus is OK
      myCAM.write_reg(ARDUCHIP_TEST1, TESTVALUE);
      delay(1);
      temp = myCAM.read_reg(ARDUCHIP_TEST1);
    
      if (temp != TESTVALUE)
      {
        Serial.println(F("ACK CMD SPI interface Error!"));
        sprintf(buff, "%d", temp);
        Serial.println(buff);
        if (CAMTimeOut++ >= 10)
        {
          digitalWrite(WHITE_LED, LOW);
          return false;
          break;
        }
          
        delay(1000);
    
        if (whiteLed)
        {
           digitalWrite(WHITE_LED, HIGH);
           whiteLed = false;
         }
         else
         {
           digitalWrite(WHITE_LED, LOW);
           whiteLed = true;
         }    
      }
      else
      {
        digitalWrite(WHITE_LED, HIGH);
        Serial.println(F("ACK CMD SPI interface OK."));break;      
      }
   }

   CAMTimeOut = 0;
  
    while(1)
    {
      //Check if the camera module type is OV5642
      myCAM.wrSensorReg16_8(0xff, 0x01);
      myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
      myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
      if((vid != 0x56) || (pid != 0x42)){
        Serial.println(F("ACK CMD Can't find OV5642 module!"));
        if (CAMTimeOut++ >= 10)
        {
          return(false);
          break;
        }
          
        delay(1000);continue;
      }
      else
      {
        Serial.println(F("ACK CMD OV5642 detected."));
        digitalWrite(BLUE_LED, HIGH);
        delay(1000);
        break;
      } 
    }
  
    //Change to JPEG capture mode and initialize the OV5642 module
    //myCAM.write_reg(ARDUCHIP_MODE, 0x00);
    myCAM.set_format(JPEG);
    myCAM.InitCAM();
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
    //myCAM.clear_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
    //myCAM.OV5642_set_JPEG_size(OV5642_320x240);
    myCAM.OV5642_set_JPEG_size(OV3640_1280x960);
    delay(2000);
    
    myCAM.clear_fifo_flag();
    myCAM.write_reg(ARDUCHIP_FRAMES,0x00);

    myCAM.clear_bit(ARDUCHIP_GPIO,GPIO_PWDN_MASK);

    return retValue;
}

bool capture()
{
  char str[8];
  File outFile;
  byte buf[256];
  static int i = 0;
  static int k = 0;
  static int n = 0;
  uint8_t temp, temp_last, timeOut = 10;
  int total_time = 0;
  bool captureFailure = false;

  //Flush the FIFO
  myCAM.flush_fifo();
  //Clear the capture done flag
  myCAM.clear_fifo_flag();
  delay(3000);
  //Start capture
  myCAM.start_capture();
  Serial.println("Start Capture");

  while (!myCAM.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK))
  {
    delay(1000);
     if (whiteLed)
     {
        digitalWrite(WHITE_LED, HIGH);
        whiteLed = false;
      }
      else
      {
        digitalWrite(WHITE_LED, LOW);
        whiteLed = true;
      }

      if (timeOut-- == 0)
      {
        captureFailure = true;
        break;
      }
  }

  Serial.println("Capture Done!");

  // Init SD card
  SDcardSetup();

  if (!captureFailure)
  {
    //Construct a file name
    k = k + 1;
    itoa(k, str, 10);
    strcat(str, ".jpg");
    //Open the new file
    outFile = SD.open(str, O_WRITE | O_CREAT | O_TRUNC);
    if (! outFile)
    {
      Serial.println("open file failed");
      //return;
    }
    else{
      Serial.println("open file successful");
    }
    total_time = millis();
  
    i = 0;
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    //temp = SPI.transfer(0x00);
    //temp_last = buf[i++] = temp = myCAM.read_fifo();
    // strip occasional spurious FF lead character
    //while ( (temp == 0xFF) & (temp_last == 0xFF) ) temp = myCAM.read_fifo(); // exit on next ! 0xFF
    //buf[i++] = temp;
  
    temp_last = buf[i++] = temp = SPI.transfer(0x00);
    // strip occasional spurious FF lead character
    while ( (temp == 0xFF) && (temp_last == 0xFF) ) temp = SPI.transfer(0x00);; // exit on next ! 0xFF
    buf[i++] = temp;
  
    //Read JPEG data from FIFO
    while ( (temp != 0xD9) | (temp_last != 0xFF) )
    {
      temp_last = temp;
      temp = SPI.transfer(0x00);;
      //Write image data to buffer if not full
      if (i < 256)
      {
        buf[i++] = temp;
      }
      else
      {
        //Write 256 bytes image data to file
        myCAM.CS_HIGH();
        outFile.write(buf, 256);
        i = 0;
        buf[i++] = temp;
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
      }
    }
    //Write the remain bytes in the buffer
    if (i > 0)
    {
      myCAM.CS_HIGH();
      outFile.write(buf, i);
    }
      
    //Close the file
    outFile.close();
    total_time = millis() - total_time;
    Serial.print("Total time used:");
    Serial.print(total_time, DEC);
    Serial.println(" millisecond");
    //Clear the capture done flag
    myCAM.clear_fifo_flag();
    digitalWrite(SD_CS, HIGH);
    myCAM.CS_HIGH();
    digitalWrite(WHITE_LED, HIGH);
  }
  else
  {
    Serial.println("Capture failure!");
    return false;
  }

  return true;
}

void getImage()
{
  File imageFile;
  uint8_t buff[RH_RF95_MAX_MESSAGE_LEN + 1]; //255 + 1
  MSGS msg = NOMESSAGE;
  uint8_t *imageBuffAddress = port->getImageBuffAdd();
  uint8_t temp, temp_last;
  int imageIndex = 0;

  //imageFile = SD.open("testFile.JPG", FILE_READ);
  imageFile = SD.open("1.JPG", FILE_READ);
  
  port->flush();
  port->sendCommand(IMAGESTART);
  delay(50);
  
  // Wait for IMAGESTARTACK
  while(port->checkMsgs() != IMAGESTARTACK){}

  imageBuffAddress[imageIndex++] = IMAGEPART; //Header
  
  //char test_buff[RH_RF95_MAX_MESSAGE_LEN]; //test
  //memset(test_buff, '\0', RH_RF95_MAX_MESSAGE_LEN);
  //sprintf(test_buff, "RH_RF95_MAX_MESSAGE_LEN is: %d", RH_RF95_MAX_MESSAGE_LEN);
  //Serial.println(test_buff);

  while ( (temp != 0xD9) | (temp_last != 0xFF) )
  {
    temp_last = temp;
    temp = imageFile.read();
    
//    sprintf(test_buff, "temp is: %d", temp);
//    Serial.println(test_buff);
    
    //Write image data to buffer if not full
    if (imageIndex < (RH_RF95_MAX_MESSAGE_LEN - 1))
    {
      imageBuffAddress[imageIndex] = temp;
      
      //sprintf(&test_buff[imageIndex], "%d", temp);
      //itoa(imageBuffAddress[imageIndex], &bufff[imageIndex], 10); //test
      imageIndex++;
    }
    else
    {
      //Write RH_RF95_MAX_MESSAGE_LEN bytes image data to master
      delay(50);
      
//      Serial.print("Sending: ");
//      sprintf(&test_buff[imageIndex], "%d", temp); //test
//      Serial.print(test_buff); //test
//      Serial.print("\n");

      port->sendImagePart(RH_RF95_MAX_MESSAGE_LEN); // Length includes header
      delay(50);
      imageIndex = 1;
      imageBuffAddress[imageIndex] = temp; // msg should have gone - so ok to start new.
      //itoa(imageBuffAddress[imageIndex], &bufff[imageIndex], 10);
      imageIndex++;

      // Wait for IMAGEPARTACK
      while(port->checkMsgs() != IMAGEPARTACK){}
    }
  }
  
  //Write the remain bytes in the buffer
  if (imageIndex > 1)
  {
      delay(50);
      port->sendImagePart(imageIndex); // Length includes header
      delay(50);
    
  }

//  for (uint8_t i = 0; i < 70; i++)
//  {
//    memset(imageBuffAddress, '\0', sizeof(imageBuffAddress));
//    imageBuffAddress[0] = IMAGEPART;
//    imageFile.seek (i * RH_RF95_MAX_MESSAGE_LEN);
//    imageFile.read (imageBuffAddress + 1, RH_RF95_MAX_MESSAGE_LEN - 1);
//    port->sendImagePart(RH_RF95_MAX_MESSAGE_LEN);
//    //delay(50);
//    
//    // Wait for IMAGEPARTACK
//    while(port->checkMsgs() != IMAGEPARTACK)
//    {
//      //delay(10);
//    }
//    delay(60);
//  }
  
  delay(1000);
  port->sendCommand(IMAGEFINISH);
  imageFile.close();
}

void createTestFile()
{
  File testFile;
  char buff[RH_RF95_MAX_MESSAGE_LEN]; //255 + 1
  
  testFile = SD.open("testFile.JPG", FILE_WRITE);

  memset(buff, '\0', RH_RF95_MAX_MESSAGE_LEN + 1);
  for(uint8_t i = 0; i < RH_RF95_MAX_MESSAGE_LEN/2; i++)
  {
    if(i < 10)
    {
      sprintf(&buff[i], "%x", i);
    }
    else
    {
      sprintf(&buff[i], "%2x", i);
    }
  }
  testFile.write(buff, RH_RF95_MAX_MESSAGE_LEN);
  testFile.write(buff, RH_RF95_MAX_MESSAGE_LEN);
  testFile.write(buff, 10);
  buff[0] = 0xFF;
  buff[1] = 0xD9;
  testFile.write(buff, 2);

  testFile.close();
}
