#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
/* include's for SD-Card */
#include "FS.h"
#include "SD.h"
#include "SPI.h"
/* include's for accel sensor */
#include <Wire.h>
#include <ADXL345_WE.h>

/*****************************************************************************************/
/*                                    GENERAL DEFINE                                     */
/*****************************************************************************************/
#define TRUE  1
#define FALSE 0
/*****************************************************************************************/
/*                                    PROJECT DEFINE                                     */
/*****************************************************************************************/
#define ADXL345_I2CADDR 0x53 // 0x1D if SDO = HIGH
#define ADLX345_FIFO_BUFFER_SIZE 16
#define LED_PIN 16
#define STRI_OUT_BUFFER_SIZE 128
#define STRI_VAL_BUFFER_SIZE 8

/*****************************************************************************************/
/*                                     TYPEDEF ENUM                                      */
/*****************************************************************************************/


/*****************************************************************************************/
/*                                   TYPEDEF STRUCT                                      */
/*****************************************************************************************/


/*****************************************************************************************/
/*                                      VARIABLES                                        */
/*****************************************************************************************/
ADXL345_WE myAcc = ADXL345_WE(ADXL345_I2CADDR);
const int int2Pin = 2;

static volatile bool int2event = FALSE;

static volatile uint8_t FirstRun = FALSE;
static volatile uint32_t LoopCounter = 0;

static char OutBuffer[STRI_OUT_BUFFER_SIZE] = {};
static char stri_loopcounter[STRI_VAL_BUFFER_SIZE] = {};
static char stri_raw_x[STRI_VAL_BUFFER_SIZE] = {};
static char stri_raw_y[STRI_VAL_BUFFER_SIZE] = {};
static char stri_raw_z[STRI_VAL_BUFFER_SIZE] = {};
static char stri_g_x[STRI_VAL_BUFFER_SIZE] = {};
static char stri_g_y[STRI_VAL_BUFFER_SIZE] = {};
static char stri_g_z[STRI_VAL_BUFFER_SIZE] = {};
static char stri_angle_x[STRI_VAL_BUFFER_SIZE] = {};
static char stri_angle_y[STRI_VAL_BUFFER_SIZE] = {};
static char stri_angle_z[STRI_VAL_BUFFER_SIZE] = {};
static char stri_corrangle_x[STRI_VAL_BUFFER_SIZE] = {};
static char stri_corrangle_y[STRI_VAL_BUFFER_SIZE] = {};
static char stri_corrangle_z[STRI_VAL_BUFFER_SIZE] = {};
static char stri_pitch[STRI_VAL_BUFFER_SIZE] = {};
static char stri_roll[STRI_VAL_BUFFER_SIZE] = {};
static char stri_orientation[STRI_VAL_BUFFER_SIZE] = {};



/*************************************************************************************************/
/**************************************************************************************************
Function: floatToString()
Argument: float f ; floating point value
          char* S ; pointer to buffer
          size_t n ; size of char buffer
          int digitsAfterDP ; digits afer the commata
return: char *
**************************************************************************************************/
/*************************************************************************************************/
char* floatToString(float f, char* S, size_t n, int digitsAfterDP) {
  if (digitsAfterDP == 0)
    snprintf(S, n, "%d", (int) (f + (f < 0 ? -0.5 : 0.5)));
  else if (digitsAfterDP < 0) {
    int i;
    for (i = 0; i < -digitsAfterDP && abs(f) >= 10; i++) f /= 10;
    char fmt[10]; // "%d%02d"
    sprintf(fmt, "%%d%%0%dd", i);
    snprintf(S, n, fmt, (int) (f + (f < 0 ? -0.5 : 0.5)), 0);
  } else {
    int M = (int) f;
    f = abs(f - (float) M);
    float g = 1;
    for (int i = digitsAfterDP; i > 0; i--) g *= 10;
    f *= g;
    f += 0.5;
    if (f >= g) {
      f -= g;
      M += 1;
    }
    int E = (int) f;
    char fmt[10]; // "%d.%05d"
    sprintf(fmt, "%%d.%%0%dd", digitsAfterDP);
    snprintf(S, n, fmt, M, E);
  }
  return(S);
}

/*************************************************************************************************/
/**************************************************************************************************
Function: replaceWord()
Argument: char* str ; pointer to string
          char* oldWord ; pointer to old word
          char* newWord ; levels to new word
return: void
**************************************************************************************************/
/*************************************************************************************************/
void replaceWord(char* str, char* oldWord, char* newWord)
{
    char *pos, temp[1000];
    int index = 0;
    int owlen;

    owlen = strlen(oldWord);

    // Repeat This loop until all occurrences are replaced.

    while ((pos = strstr(str, oldWord)) != NULL) {
        // Bakup current line
        strcpy(temp, str);

        // Index of current found word
        index = pos - str;

        // Terminate str after word found index
        str[index] = '\0';

        // Concatenate str with new word
        strcat(str, newWord);

        // Concatenate str with remaining words after
        // oldword found index.
        strcat(str, temp + index + owlen);
    }
}

/*************************************************************************************************/
/**************************************************************************************************
Function: reverse()
  Reverses a string 'str' of length 'len' 
Argument: char* str ; pointer to string
          int len ; length of string
return: void
**************************************************************************************************/
/*************************************************************************************************/
void reverse(char* str, int len) 
{ 
    int i = 0, j = len - 1, temp; 
    while (i < j) { 
        temp = str[i]; 
        str[i] = str[j]; 
        str[j] = temp; 
        i++; 
        j--; 
    } 
} 

/*************************************************************************************************/
/**************************************************************************************************
Function: intToStr()
 Converts a given integer x to string str[]. 
 d is the number of digits required in the output. 
 If d is more than the number of digits in x, 
 then 0s are added at the beginning. 
Argument: int x ; integer to convert
          char str[] ; pointer to output string
          int d ; d is the number of digits required in the output
return: int
**************************************************************************************************/
/*************************************************************************************************/
int intToStr(int x, char str[], int d) 
{ 
    int i = 0; 
    while (x) { 
        str[i++] = (x % 10) + '0'; 
        x = x / 10; 
    } 

    // If number of digits required is more, then 
    // add 0s at the beginning 
    while (i < d) 
        str[i++] = '0'; 

    reverse(str, i); 
    str[i] = '\0'; 
    return i; 
} 

/*************************************************************************************************/
/**************************************************************************************************
Function: listDir()
Argument: fs::FS &fs ; file system handle
          const char *dirname ; pointer to directory
          uint8_t levels ; levels to display
return: void
**************************************************************************************************/
/*************************************************************************************************/
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

/*************************************************************************************************/
/**************************************************************************************************
Function: createDir()
Argument: fs::FS &fs ; file system handle
          const char *path ; path to file
return: void
**************************************************************************************************/
/*************************************************************************************************/
void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

/*************************************************************************************************/
/**************************************************************************************************
Function: removeDir()
Argument: fs::FS &fs ; file system handle
          const char *path ; path to file
return: void
**************************************************************************************************/
/*************************************************************************************************/
void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

/*************************************************************************************************/
/**************************************************************************************************
Function: readFile()
Argument: fs::FS &fs ; file system handle
          const char *path ; path to file
return: void
**************************************************************************************************/
/*************************************************************************************************/
void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

/*************************************************************************************************/
/**************************************************************************************************
Function: writeFile()
Argument: fs::FS &fs ; file system handle
          const char *path ; path to file
          const char *message ; pointer to message
return: void
**************************************************************************************************/
/*************************************************************************************************/
void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

/*************************************************************************************************/
/**************************************************************************************************
Function: appendFile()
Argument: fs::FS &fs ; file system handle
          const char *path ; path to file
          const char *message ; pointer to message
return: void
**************************************************************************************************/
/*************************************************************************************************/
void appendFile(fs::FS &fs, const char *path, const char *message) {
  // Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
     //Serial.println("Message appended");
   } else {
     Serial.println("Append failed");
   }
  file.close();
}

/*************************************************************************************************/
/**************************************************************************************************
Function: openFile()
Argument: fs::FS &fs ; file system handle
          const char *path ; path to file
return: void
**************************************************************************************************/
/*************************************************************************************************/
uint8_t openFile(fs::FS &fs, const char *path)
{
  uint8_t return_val = FALSE;

  // Serial.printf("open file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("open file failed");
  }
  else
  {
    return_val = TRUE;
  }
  
  file.close();
  return return_val;
}

/*************************************************************************************************/
/**************************************************************************************************
Function: renameFile()
Argument: fs::FS &fs ; file system handle
          const char *path1 ; path to source
          const char *path2 ; path to destination
return: void
**************************************************************************************************/
/*************************************************************************************************/
void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

/*************************************************************************************************/
/**************************************************************************************************
Function: deleteFile()
Argument: fs::FS &fs ; file system handle
          const char *path ; path to file
return: void
**************************************************************************************************/
/*************************************************************************************************/
void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

/*************************************************************************************************/
/**************************************************************************************************
Function: testFileIO()
Argument: fs::FS &fs ; file system handle
          const char *path ; path to payload
return: void
**************************************************************************************************/
/*************************************************************************************************/
void testFileIO(fs::FS &fs, const char *path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file) {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len) {
      size_t toRead = len;
      if (toRead > 512) {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %lu ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
  file.close();
}

/*************************************************************************************************/
/**************************************************************************************************
Function: setup()
return: void
**************************************************************************************************/
/*************************************************************************************************/
void setup() {
  uint8_t cardType = CARD_NONE;
  uint64_t cardSize = 0;

  /****************** Initializing ********************/
  Serial.begin(115200);
  Wire.begin(); /* Initialize I2C */
  /* build in LED */
  pinMode(LED_PIN, OUTPUT);

  pinMode(int2Pin, INPUT);


  /************* Initializing SD-Card *****************/
  Serial.println("SD-Card Initializing start...");
  if (!SD.begin()) {

    Serial.println("Card Mount Failed");
    return;
  }
  cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
  Serial.println("SD-Card Initializing end...");
  Serial.println();

  /*********** Initializing Accel Sensor **************/
  Serial.println("ADXL345 Initializing start...");
  if(!myAcc.init()){
    Serial.println("ADXL345 not connected!");
  }

  /* set correction factors */
  myAcc.setCorrFactors(-255.0, 263.0, -252.0, 267.0, -257.0, 246);
  
/* In this next step the offset for angles is corrected to get quite precise corrected 
    angles x and y up to ~60°. The additional offsetCorrection is only for corrected 
    angle measurements, not for pitch and roll. The procedure just ensures to start at 0°.
*/
  Serial.println("Position your ADXL345 flat and don't move it");
  delay(2000);
  myAcc.measureAngleOffsets();

  myAcc.setDataRate(ADXL345_DATA_RATE_3200);
  delay(100);
  Serial.println(myAcc.getDataRateAsString());
  myAcc.setRange(ADXL345_RANGE_2G);
  Serial.print("  /  g-Range: ");
  Serial.println(myAcc.getRangeAsString());

  attachInterrupt(digitalPinToInterrupt(int2Pin), event2ISR, RISING);

  
/* You can choose the following interrupts:
     Variable name:             Triggered, if:
    ADXL345_OVERRUN      -   new data replaces unread data
    ADXL345_WATERMARK    -   the number of samples in FIFO equals the number defined in FIFO_CTL
    ADXL345_FREEFALL     -   acceleration values of all axes are below the threshold defined in THRESH_FF 
    ADXL345_INACTIVITY   -   acc. value of all included axes are < THRESH_INACT for period > TIME_INACT
    ADXL345_ACTIVITY     -   acc. value of included axes are > THRESH_ACT
    ADXL345_DOUBLE_TAP   -   double tap detected on one incl. axis and various defined conditions are met
    ADXL345_SINGLE_TAP   -   single tap detected on one incl. axis and various defined conditions are met
    ADXL345_DATA_READY   -   new data available

    Assign the interrupts to INT1 (INT_PIN_1) or INT2 (INT_PIN_2). Data ready, watermark and overrun are 
    always enabled. You can only change the assignment of these which is INT1 by default.

    You can delete interrupts with deleteInterrupt(type);
*/ 
  myAcc.setInterrupt(ADXL345_WATERMARK, INT_PIN_1); // Interrupt when FIFO is full
  
/* The following two FIFO parameters need to be set:
    1. Trigger Bit: not relevant for this FIFO mode.
    2. FIFO samples (max 32). Defines the size of the FIFO. One sample is an x,y,z triple.
*/
  myAcc.setFifoParameters(ADXL345_TRIGGER_INT_1, ADLX345_FIFO_BUFFER_SIZE);

/* You can choose the following FIFO modes:
    ADXL345_FIFO     -  you choose the start, ends when FIFO is full (at defined limit)
    ADXL345_STREAM   -  FIFO always filled with new data, old data replaced if FIFO is full; you choose the stop
    ADXL345_TRIGGER  -  FIFO always filled up to 32 samples; when the trigger event occurs only defined number of samples
                        is kept in the FIFO and further samples are taken after the event until FIFO is full again. 
    ADXL345_BYPASS   -  no FIFO
*/   
  myAcc.setFifoMode(ADXL345_FIFO); 
  myAcc.readAndClearInterrupts();
  
  delay(100);
  Serial.println("ADXL345 Initializing end...");
  Serial.println();
}

/*************************************************************************************************/
/**************************************************************************************************
Function: loop()
return: void
**************************************************************************************************/
/*************************************************************************************************/
void loop() 
{
  xyzFloat raw;
  xyzFloat g;
  xyzFloat angle;
  xyzFloat corrangle;
  float pitch;
  float roll;
  byte intType;
  String str_orientation;

   /* check for first run */
  if(FirstRun == FALSE)
  {
    writeFile(SD, "/logging.txt", "count; raw-x; raw-y; raw-z; g-x; g-y; g-z; angle-x; angle-y; angle-z; corr-angle-x; corr-angle-y; corr-angle-z; pitch; roll; orientation\n");
    Serial.println("Start Logging...");
    /* build in LED */
    digitalWrite(LED_PIN, TRUE);
    LoopCounter = 0;
    FirstRun = TRUE;

    myAcc.readAndClearInterrupts();
    myAcc.setMeasureMode(true); // this is the actual start

  } 

  while(!int2event){}
  myAcc.setMeasureMode(false); // this is the actual stop
  int2event = FALSE;

  intType = myAcc.readAndClearInterrupts();
  if (myAcc.checkInterrupt(intType, ADXL345_OVERRUN))
  {
    Serial.println("INT2 OVERRUN");
  }
  else if (myAcc.checkInterrupt(intType, ADXL345_WATERMARK))
  {
    //Serial.println("INT2 WATERMARK");
  }
  else if(myAcc.checkInterrupt(intType, ADXL345_FREEFALL))
  {
    Serial.println("INT2 FREEFALL");
  }
  else if (myAcc.checkInterrupt(intType, ADXL345_INACTIVITY))
  {
    Serial.println("INT2 INACTIVITY");
  }
  else if (myAcc.checkInterrupt(intType, ADXL345_ACTIVITY))
  {
    Serial.println("INT2 ACTIVITY");
  }
  else if (myAcc.checkInterrupt(intType, ADXL345_DOUBLE_TAP))
  {
    Serial.println("INT2 DOUBLE_TAP");
  }  
  else if (myAcc.checkInterrupt(intType, ADXL345_SINGLE_TAP))
  {
    Serial.println("INT2 SIGLE_TAP");
  } 
  else if (myAcc.checkInterrupt(intType, ADXL345_DATA_READY))
  {
    Serial.println("INT2 DATA_READY");
  }
  else
  {
    Serial.print("INT2 UNKNOWN: ");
    Serial.println(intType);
  }

  for(int i=0; i < ADLX345_FIFO_BUFFER_SIZE; i++)
  { 
    myAcc.getRawValues(&raw);
    myAcc.getGValues(&g);
    myAcc.getAngles(&angle);
    myAcc.getCorrAngles(&corrangle);
    pitch = myAcc.getPitch();
    roll  = myAcc.getRoll();
    str_orientation = myAcc.getOrientationAsString();


    /* clear buffer's first */
    memset(&OutBuffer[0],'/0',STRI_OUT_BUFFER_SIZE);
    memset(&stri_loopcounter[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_raw_x[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_raw_y[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_raw_z[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_g_x[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_g_y[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_g_z[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_angle_x[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_angle_y[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_angle_z[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_corrangle_x[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_corrangle_y[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_corrangle_z[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_pitch[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_roll[0],'/0',STRI_VAL_BUFFER_SIZE);
    memset(&stri_orientation[0],'/0',STRI_VAL_BUFFER_SIZE);
    
    /* Add leading zeros to the counter */
    intToStr(LoopCounter, stri_loopcounter, 7);
    for(int u=0; u < STRI_VAL_BUFFER_SIZE; u++){
      stri_orientation[u] = str_orientation.charAt(u);
    }

    /* convert float to string with 2 decimal after comma values */
    floatToString(raw.x, stri_raw_x, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(raw.y, stri_raw_y, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(raw.z, stri_raw_z, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(g.x, stri_g_x, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(g.y, stri_g_y, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(g.z, stri_g_z, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(angle.x, stri_angle_x, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(angle.y, stri_angle_y, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(angle.z, stri_angle_z, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(corrangle.x, stri_corrangle_x, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(corrangle.y, stri_corrangle_y, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(corrangle.z, stri_corrangle_z, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(pitch, stri_pitch, STRI_VAL_BUFFER_SIZE, 2);
    floatToString(roll, stri_roll, STRI_VAL_BUFFER_SIZE, 2);

    /* fill buffer with the counter, raw values and g-values */
    strcpy(OutBuffer, stri_loopcounter);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_raw_x);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_raw_y);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_raw_z);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_g_x);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_g_y);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_g_z);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_angle_x);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_angle_y);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_angle_z);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_corrangle_x);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_corrangle_y);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_corrangle_z);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_pitch);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_roll);
    strcat(OutBuffer, "; ");
    strcat(OutBuffer, stri_orientation);
    strcat(OutBuffer, "\n");

    /* replacement of dots to commata */
    replaceWord(OutBuffer, ".",",");

    /* output each buffer */
    //printf("%s", &OutBuffer[0]);
    
    // Serial.print("Raw-x = ");
    // Serial.print(raw.x);
    // Serial.print("  |  Raw-y = ");
    // Serial.print(raw.y);
    // Serial.print("  |  Raw-z = ");
    // Serial.println(raw.z);

    // Serial.print("g-x   = ");
    // Serial.print(g.x);
    // Serial.print("  |  g-y   = ");
    // Serial.print(g.y);
    // Serial.print("  |  g-z   = ");
    // Serial.println(g.z);

    // /* Angles use the corrected raws. Angles are simply calculated by
    // angle = arcsin(g Value) */
    // Serial.print("Angle x  = ");
    // Serial.print(angle.x);
    // Serial.print("  |  Angle y  = ");
    // Serial.print(angle.y);
    // Serial.print("  |  Angle z  = ");
    // Serial.println(angle.z);

    // Serial.print("Angle x = ");
    // Serial.print(corrangle.x);
    // Serial.print("  |  Angle y = ");
    // Serial.print(corrangle.y);
    // Serial.print("  |  Angle z = ");
    // Serial.println(corrangle.z);

    // pitch = myAcc.getPitch();
    // roll  = myAcc.getRoll();
    
    // Serial.print("Pitch   = "); 
    // Serial.print(pitch); 
    // Serial.print("  |  Roll    = "); 
    // Serial.println(roll); 
    // Serial.println();

    // Serial.print("Orientation of the module: ");
    // Serial.println(myAcc.getOrientationAsString());
    // Serial.println();
      
    /* check for open file */
    if(openFile(SD, "/logging.txt") == TRUE)
    {
      /* add data to the file */
      appendFile(SD, "/logging.txt", OutBuffer);
    }
      
    LoopCounter++; 
    /* check if FIFO is empty */
    // if(myAcc.getFifoStatus() == 0){
    //   Serial.print("FIFO end");
    // break;
    //}

  } /* end loop */

  /* print every 512th Buffer */
  if(LoopCounter % 512 == 0 )
  {
    printf("%s", &OutBuffer[0]);
  }

  myAcc.resetTrigger();
  myAcc.readAndClearInterrupts();
  myAcc.setMeasureMode(true); // this is the actual start
}

void event2ISR() {
  int2event = TRUE;
}
