/////////////////////////////////////////////
// log time when motion is detected
// J Marais
// 24/05/2025
/////////////////////////////////////////////
//////////////////////////////////////// time
#include <Arduino.h>
#include <WiFi.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include <Wire.h>
#include "time.h"
#include "esp_sntp.h" // get network time
#include "SensorQMI8658.hpp"
#include "MadgwickAHRS.h"  // Install from https://github.com/arduino-libraries/MadgwickAHRS
#include "esp_timer.h"  // for hardware timer
#include <SPIFFS.h>
#include <FS.h>   // file handling
const char *ssid = "XYZ123456789";
const char *password = "netJohanweet";

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0; //3600;
static int16_t hours, minutes, sec;
unsigned int SecCounter = 0;
static int16_t w, h, center;
IMUdata acc, gyr;

//////////////////////////////////////// Display
HWCDC USBSerial;
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST /* RST */,
                                      0 /* rotation */, true /* IPS */, LCD_WIDTH, LCD_HEIGHT, 0, 20, 0, 0);

SensorQMI8658 qmi;
Madgwick filter;
////////////////////////// Interrupt routine to be called at 25 Hz
// Interrupt Service Routine (ISR) for 25Hz execution

void IRAM_ATTR onTimer(void* arg) {  // - IRAM_ATTR tells the ESP32-S3 to store this function in RAM (for faster execution)
    // Read accelerometer and gyroscope data
    qmi.getAccelerometer(acc.x, acc.y, acc.z);
    qmi.getGyroscope(gyr.x, gyr.y, gyr.z);
    // Update Madgwick filter with sensor data
    filter.updateIMU(gyr.x, gyr.y, gyr.z, acc.x, acc.y, acc.z);
}

/////////////////////////////////////////////////////////////
void setup() {
  // Create timer
  esp_timer_handle_t timer;
  esp_timer_create_args_t timer_args = {
      .callback = &onTimer,
      .name = "25HzTimer"
  };  
  esp_timer_create(&timer_args, &timer);
  esp_timer_start_periodic(timer, 40000); // 40ms interval = 25Hz

  WiFi.begin(ssid, password);
  USBSerial.begin(115200);
  USBSerial.println("Motion Logger");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    USBSerial.print(".");
  }
  USBSerial.println("WiFi CONNECTED");
  // Init Display
  if (!gfx->begin())
  {
    USBSerial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  // init LCD constant
  w = gfx->width();
  h = gfx->height();
  // set Time notification call-back function
  sntp_set_time_sync_notification_cb(timeavailable);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    USBSerial.println("waiting for time...");
    delay(500);
  }
  // Extract hours and minutes
  hours = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;
  sec = timeinfo.tm_sec;
  gfx->setCursor(20, h-50);
  gfx->setTextColor(random(0xffff));
  gfx->setTextSize(5 /* x scale */, 5 /* y scale */, 3 /* pixel_margin */);
//////// check strage space availability
  if (!SPIFFS.begin(true)) {
      USBSerial.println("SPIFFS Mount Failed!");
      return;
  }
    // Get total and used space
    
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  USBSerial.printf("Total Storage: %d bytes\n", totalBytes);
  USBSerial.printf("Used Storage: %d bytes\n", usedBytes);
  USBSerial.printf("Free Space: %d bytes\n", totalBytes - usedBytes);
  printLogFile("/datalog.txt");  // Read and display the log file
  // Initialize QMI8658 sensor
  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS)) {
      USBSerial.println("Failed to initialize QMI8658!");
      while (1);
  }
  qmi.configAccelerometer(
      SensorQMI8658::ACC_RANGE_4G,
      SensorQMI8658::ACC_ODR_31_25Hz,
      SensorQMI8658::LPF_MODE_3);
  qmi.configGyroscope(
      SensorQMI8658::GYR_RANGE_64DPS,
      SensorQMI8658::GYR_ODR_28_025Hz,
      SensorQMI8658::LPF_MODE_3);
  qmi.enableGyroscope();
  qmi.enableAccelerometer();
  filter.begin(25);  // Set filter update rate to 25Hz
}
//////////////////////////////////////////////////////////////////////////
// Callback function (gets called when time adjusts via NTP)
void timeavailable(struct timeval *t) {
  //USBSerial.println("Got time adjustment from NTP!");
  printLocalTime();
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    USBSerial.println("No time available (yet)");
    return;
  }
  // Extract hours and minutes
  hours = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;
  sec = timeinfo.tm_sec;
  //USBSerial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  //USBSerial.print(hours);
  //USBSerial.print(": ");
  //USBSerial.println(minutes);
  // Clear the previous time area by filling a small rectangle
  gfx->fillRect(20, h-50, LCD_WIDTH, 50, BLACK);  // Clear the area for the time
  gfx->setCursor(20, h-50);
  //gfx->setTextColor(random(0xffff));
  gfx->setTextSize(5 /* x scale */, 5 /* y scale */, 3 /* pixel_margin */);
  gfx->print(hours);
  gfx->print(":");
  gfx->println(minutes);
}
///////////////////////////////////// Data logging
void logData(String data) {
    File file = SPIFFS.open("/datalog.txt", FILE_APPEND);  // Open file in append mode
    if (file) {  // Check if the file opened successfully
        file.println(data);  // Write data to the file
        file.close();  // Close the file to save changes
    } else {
        USBSerial.println("Error opening file!");  // Error handling
    }
}
////////////////////////////////////////////////////////////////////////
void loop() {
  // put your main code here, to run repeatedly:
    printLocalTime();  // it will take some time to sync time :)

    // Retrieve Euler angles
    float roll  = filter.getRoll();
    float pitch = filter.getPitch();
    float yaw   = filter.getYaw();
  // display orientation
    gfx->fillRect(20, 20, LCD_WIDTH-20, 75, BLACK);  // Clear the area for the time
    gfx->setTextSize(2 /* x scale */, 2 /* y scale */, 2 /* pixel_margin */);
    gfx->setCursor(20, 20);
    gfx->print((int)roll);
    gfx->setCursor(20, 20+20);
    gfx->print((int)pitch);  
    gfx->setCursor(20, 20+40);
    gfx->print((int)yaw);

    sleep(1);
    SecCounter+=1;
    if (SecCounter>=60){  // save data every 60 seconds
      String data = String(hours)+":"+String(minutes)+":"+String(sec)+"; roll: "+String(roll)+"; pitch:"+String(pitch)+"; yaw:"+String(yaw);
      logData(data);
      SecCounter = 0;
    }
}

////////////////////////////// print the whole log file to USB
void printLogFile(const char* filename) {
    File file = SPIFFS.open(filename, FILE_READ);  // Open log file for reading

    if (!file) {
        USBSerial.println("Failed to open file for reading!");
        return;
    }

    USBSerial.println("---- Datalog Contents ----");
    while (file.available()) {
        USBSerial.write(file.read());  // Read byte-by-byte and print
    }
    USBSerial.println("\n--------------------------");

    file.close();  // Close the file
}