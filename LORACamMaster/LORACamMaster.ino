#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include <SD.h>
#include <loraPortClass.h>

#define SD_CS 10
#define WHITE_LED 12
#define BLUE_LED 11
#define YELLOW_LED 9
#define RED_LED 13

bool yellowLed = true;
bool whiteLed = false;
bool blueLed = false;
bool redLed = false;
bool sendImageStartAck = false;
bool sendImagePartAck = false;

loraPortClass *LoraPort = NULL;

Sd2Card card;
File myFile;
int fileIndex = 0;
int image_start_ack_guard_timer = 0;
int image_part_ack_guard_timer = 0;

bool SDcardSetup();
void readCard();
void imagePart();

void setup() {
  // put your setup code here, to run once:
  pinMode(WHITE_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  delay(10000);
  Serial.begin(115200);
  Serial.println("Hello Paul!");
  // initialize SPI:
  SPI.begin();

  if (!SDcardSetup())
  {
    Serial.println("Not ready - card setup failure!");
    digitalWrite(WHITE_LED, HIGH);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(BLUE_LED, HIGH);
  }

  LoraPort = new loraPortClass(portClass::master, 10, 6);
  LoraPort->start();
}

char buff[100];

void loop() {
  // put your main code here, to run repeatedly:
  
  if (Serial.available()) 
  {
    int value = Serial.read();
    sprintf(buff, "rxed something %d", value);
    switch (value - 48)
    {
      case 0:
          Serial.println("SD Card setup");
          SDcardSetup();
          break;
      case 1:
          Serial.println("Reading card");
          readCard();
          break;
      case 2:
          Serial.println("Sending test IMAGESTARTACK");
          delay(30000);
          LoraPort->sendCommand(IMAGESTARTACK);
          break;
      case 3:
          Serial.println("Sending test IMAGEPARTACK");
          delay(30000);
          LoraPort->sendCommand(IMAGEPARTACK);
          break;
      //case CAMON: //3 - CAM ON
      //    break;
      case CAMOFF: //4 - CAM OFF
          break;
      case CAPTURE: //5 - CAPTURE
          sprintf(buff, "Sending command: %d", value - 48);
          Serial.println(buff);
          LoraPort->sendCommand(CAPTURE);
          break;
      case GETIMAGE: //9
          sprintf(buff, "Sending command: %d", value - 48);
          Serial.println(buff);
          delay(30000);
          //LoraPort->setModeTx();
          LoraPort->flush(); //Clear Lora of any received messages.
          LoraPort->sendCommand(GETIMAGE);
          image_start_ack_guard_timer = millis() + 5000;
          sendImageStartAck = true;
          break;
      default:
        sprintf(buff, "rxed something invalid %d", value - 48);
        Serial.println(buff);
        break;
    }
  }

  // Pulse
  MSGS msg = LoraPort->checkMsgs();

  switch(msg)
  {
    case CAPTURECOMPLETE:
      Serial.println("CAPTURECOMPLETE rxed");
      break;
    case CAPTUREFAILED:
      Serial.println("CAPTUREFAILED rxed");
      break;
    case IMAGESTART:
      myFile = SD.open("1.JPG", FILE_WRITE);
      Serial.println("IMAGESTART rxed");
      delay(50);
      LoraPort->sendCommand(IMAGESTARTACK);
      fileIndex = 0;
      image_start_ack_guard_timer = millis() + 5000;
      sendImageStartAck = true;
      break;
    case IMAGEPART:
      Serial.println("IMAGEPART rxed");
      imagePart();
      delay(50);
      LoraPort->sendCommand(IMAGEPARTACK);
      sendImagePartAck = true;
      image_part_ack_guard_timer = millis() + 5000;
      sendImageStartAck = false;
      break;
    case PING0:
    case PING1:
    case CAPTUREACK:
      break;
    case IMAGEFINISH:
      Serial.println("IMAGEFINISH rxed");
      imagePart();
      delay(50);
      LoraPort->sendCommand(IMAGEFINISHACK);
      myFile.close();
      sendImagePartAck = false;
      break;
    case GETIMAGEACK:
    case NOMESSAGE:
      break;
    default:
      Serial.println("Lora Port error!");
      break;
  }
  
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

  if ((millis() > image_start_ack_guard_timer) && sendImageStartAck)
  {
    LoraPort->sendCommand(IMAGESTARTACK);
    image_start_ack_guard_timer += 5000;
  }

  if ((millis() > image_part_ack_guard_timer) && sendImagePartAck)
  {
    LoraPort->sendCommand(IMAGEPARTACK);
    image_part_ack_guard_timer += 5000;
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

void imagePart()
{
  Serial.println("Debug: imagePart() has been called");
  uint8_t *imageBuffAddress = LoraPort->getRecBuffAddress();
  uint8_t msg_length = LoraPort->getMsgLength() - 1; //Ignore 1st byte header

//  char testBuf[RH_RF95_MAX_MESSAGE_LEN];
//  sprintf(testBuf, "msg_length is %d", msg_length);
//  Serial.println(testBuf);

//  char buff[RH_RF95_MAX_MESSAGE_LEN];
//  for(int i = 0; i < msg_length; i++)
//  {
//    itoa(imageBuffAddress[i], &buff[i], 16);
//  }
//  Serial.println(buff);
  
  myFile.seek (fileIndex);
  myFile.write(imageBuffAddress + 1, msg_length); //[0] has the header. Ignore it.
  fileIndex += msg_length;
}
