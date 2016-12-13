#include <Adafruit_9DOF.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM303_U.h>
#include <Adafruit_L3GD20_U.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library (you most likely already have this in your sketch)
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <EEPROM.h>

#define OLED_RESET 0
Adafruit_SSD1306 display(OLED_RESET);

#define DELTAY 2
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
#define MAX_PITCHES 10

static float _lsm303Accel_MG_LSB     = 0.012F;   // 1, 2, 4 or 12 mg per lsb
static float _lsm303Mag_Gauss_LSB_XY = 1100.0F;  // Varies with gain
static float _lsm303Mag_Gauss_LSB_Z  = 980.0F;   // Varies with gain

/* Assign a unique ID to this sensor at the same time */
Adafruit_L3GD20_Unified gyro = Adafruit_L3GD20_Unified(20);
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(54321);
Adafruit_LSM303_Mag_Unified mag = Adafruit_LSM303_Mag_Unified(12345);


/* wifi */
const char* host = "api.thingspeak.com";
const char* writeAPIKey = "1LVSR0DQW0DP37HK"; // <-- put your API Key here
const char* tweeterHost = "https://api.thingspeak.com/apps/thingtweet/1/statuses/update";
const char* thingtweetAPIKey = "Q1NCP7LYSXMK9VRM";

WiFiClient client;
const int httpPort = 80;

/* system mode */
int sys_mode = 0;

/* SD card */
File myFile;
uint16_t sd_buffer[6 * 256];
int count = 0;
volatile int state = 0;
int file_open = 0;

/*Prediction declerations*/
#define DEAD_ZONE_VALUE  (10 / 0.001221731 )
#define DEAD_ZONE_COUNTER 300
#define EVENT_COUNTER 10
#define GYRO_INTENSITY_FOR_PITCH ( 20 / 0.001221731 )
int16_t maxGyroYVal;
int16_t maxGyroXVal;
int16_t maxGyroZVal;

int pitchState;
int deadZoneCounter;
uint8_t pitchCount;
int eventCounter;
int last_speed = 0;
uint8_t maxEffortPitchCount;
uint8_t game_mode = 0; /* 0: pactice, 1: match
                        * this is encoded into the speed of each pitch
                        */

uint16_t accel_count = 0;
uint16_t acc_count = 0;
uint16_t dac_count = 0;
uint16_t acc_time = 0;
uint16_t dac_time = 0;
bool dac_updated = false;

int max_speed = 0;

void change_mode() {
  static unsigned long last = 0;
  unsigned long now = millis();

  if ( now - last < 500 ) /* debounce */
    return;

  last = now;
  if ( !game_mode )
  {
    game_mode = 1;
  }
  else
    game_mode = 0;
}

void setup_OLED()
{
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  // init done

  display.clearDisplay();
  init_drawchar();
  delay(1000);
  draw_text("don't try too hard");
}

void init_drawchar(void) {
  char msg[] = "You are   awesome";

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  for (uint8_t i = 0; i < strlen(msg); i++) {
    display.write(msg[i]);
  }
  display.display();
  delay(1);
}

void draw_text( String s) {
  display.clearDisplay();
  display.setCursor(0, 0);
  for (uint8_t i = 0; i < s.length(); i++) {
    display.write(s[i]);
  }
  display.display();
}

void draw_speed(int v) {
  char msg[] = "Est Speed:";
  if ( v >= 95 ) /* limit speed display to double digits */
    v = 94;

  String speedL(v > 5 ? v - 5 : 0);
  String speedH(v + 5);

  display.clearDisplay();
  display.setCursor(0, 0);

  for (uint8_t i = 0; i < strlen(msg); i++) {
    display.write(msg[i]);
  }
  display.println();
  for (uint8_t i = 0; i < speedL.length(); i++) {
    display.write(speedL[i]);
  }
  display.write('~');
  for (uint8_t i = 0; i < speedH.length(); i++) {
    display.write(speedH[i]);
  }
  display.write(' '); display.write('m'); display.write('p'); display.write('h');
  display.display();
}

void draw_pitches() {
  char msg[] = "P:";
  char msg2[] = "Stop      pitching!";
  int diff = MAX_PITCHES - pitchCount;
  static uint8_t cur_game_mode = 2;
  char new_rec[] = "New record";

  if ( cur_game_mode != game_mode || cur_game_mode == 2)
  {
    char prac_mode[] = "Practice  mode";
    char match_mode[] = "Match     mode";
    
    char* mode_s;
    cur_game_mode = game_mode;
    
    if ( !cur_game_mode )
        mode_s = prac_mode;
    else
        mode_s = match_mode;
    
    display.clearDisplay();
    display.setCursor(0, 0);

    for (uint8_t i = 0; i < strlen(mode_s); i++) {
      display.write(mode_s[i]);
    }
    display.display();
    delay(1000);
  }
  
  display.clearDisplay();
  display.setCursor(0, 0);
  if ( diff > 0 )
  {
    String pitches(pitchCount);
    String speedString(last_speed);
    String accT(acc_time);
    String dacT(dac_time);

    for (uint8_t i = 0; i < strlen(msg); i++) {
      display.write(msg[i]);
    }

    for (uint8_t i = 0; i < pitches.length(); i++) {
      display.write(pitches[i]);
    }
    display.write(',');
    
    for (uint8_t i = 0; i < speedString.length(); i++) {
      display.write(speedString[i]);
    }
    display.println();

    display.drawRect(0, 16, 128, 16, 1);
    if ( max_speed && last_speed )
      if ( max_speed == last_speed )
        for (uint8_t i = 0; i < strlen(new_rec); i++)
          display.write(new_rec[i]);
      else
        display.fillRect(3, 19, (int)((double)(last_speed - 35) / ( max_speed -35 ) * 122) , 10, 1);
    /* the acc and dac time
     * for (uint8_t i = 0; i < accT.length(); i++) {
      display.write(accT[i]);
    }
    display.write(',');
    for (uint8_t i = 0; i < dacT.length(); i++) {
      display.write(dacT[i]);
    }*/
    
  }
  else
  {

    for (uint8_t i = 0; i < strlen(msg2); i++) {
      display.write(msg2[i]);
    }
  }

  display.display();

  
}

void setup(void)
{
  // predictions setup
  maxGyroYVal = 0;
  maxGyroXVal = 0;
  maxGyroZVal = 0;
  pitchState = 0;
  deadZoneCounter = 0;

  eventCounter = 0;

  pinMode(2, INPUT);

  pinMode(1,OUTPUT);
  digitalWrite(1,HIGH);
/*
#ifndef ESP8266
  while (!Serial);     // will pause Zero, Leonardo, etc until serial console opens
#endif
  Serial.begin(9600);
*/
  EEPROM.begin(512);

  setup_OLED();
  delay(2000);
/*
      for ( int i = 0; i < 512; i++ )
    {
      Serial.print(EEPROM.read(i), HEX);
      EEPROM.write(i,0);

    }
    EEPROM.commit();
  */
  pitchCount = EEPROM.read(0);
  max_speed = EEPROM.read(1);
  maxEffortPitchCount = EEPROM.read(2);
/*
  Serial.print("session pitch count: ");
  Serial.println(pitchCount);
  Serial.print("maxSpeed: ");
  Serial.println(max_speed);
  Serial.print("maxEffortPitchCount:  ");
  Serial.println(maxEffortPitchCount );
*/
  int button_count = 0;
  while ( digitalRead(2) == 0 )
  {
    button_count++;
    delay(10);
  }

  if ( button_count > 400 )
  {
    draw_text("Resetting PitchCount");
    pitchCount = 0;
    EEPROM.write(0, pitchCount);
    EEPROM.write(2, 0); /* resetting max effort in this session */
    delay(3000);
    EEPROM.commit();
  }
  else if ( button_count > 0 )
  {
    /* start wifi hotspot or connect to AP */
    sys_mode = 1;
    draw_text("Running wifi...");
    WiFiManager wifiManager; /* thank you Jesus */

    wifiManager.autoConnect("Cha Cha's hotspot");
    return;
  }

  /* Initialise the sensor */
  if (!accel.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
    draw_text(String("LSM303 a\n"));
    while (1);
  }

  /* Enable auto-gain
    mag.enableAutoRange(true);*/

  /* Initialise the sensor */
  /*if (!mag.begin())
    {
    draw_text(String("LSM303 m\n"));
    while (1)
      Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
    }
  */
  /* Enable auto-ranging */
  //gyro.enableAutoRange(true);

  /* Initialise the sensor */
  if (!gyro.begin(GYRO_RANGE_2000DPS))
  {
    /* There was a problem detecting the L3GD20 ... check your connections */
    draw_text(String("L3GD20\n"));
    while (1);
    Serial.println("Ooops, no L3GD20 detected ... Check your wiring!");
  }
  /*
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.println("trying to connect to wifi...");
    }
    Serial.println("connected");
  */
  while (!SD.begin(15)) {
    draw_text(String("SD\n"));
    Serial.println("retrying SD card");
    delay
    (2000);
  }
  

  

  attachInterrupt(2, change_mode, FALLING);

}

int16_t absVal(int16_t val) {
  if (val < 0)
    return -val;
  return val;
}

//max sensor reading loop speed 537hz

void loop(void)
{
  bool s = HIGH; 
  if ( sys_mode )
  {
    if (!client.connect(host, httpPort)) {
      return;
    }

    String url = "/update?key=";
    url += writeAPIKey;
    url += "&field1=";
    url += String(pitchCount);

    String speedString, accString, dacString;
    /* looping through le EEPROM */
    for ( int i = 0; i < pitchCount; i++ )
    {
        speedString += String(EEPROM.read(3 + i * 5));

        uint16_t tmp = ((uint16_t)EEPROM.read(3 + i * 5 + 1)) << 8;
        tmp |= EEPROM.read(3 + i * 5 + 2);

        accString += String(tmp);

        tmp = ((uint16_t)EEPROM.read(3 + i * 5 + 3)) << 8;
        tmp |= EEPROM.read(3 + i * 5 + 4);
        dacString += String(tmp);
        if ( i != pitchCount - 1 )
        {
            speedString+=String(",");
            accString+=String(",");
            dacString+=String(",");
        }
    }
/*
    Serial.println(speedString);
    Serial.println(accString);
    Serial.println(dacString);
*/
    url += "&field2="; /* speed: s1, s2, s3... */
    url += speedString; 
    url += "&field3="; /* acc1, acc2, acc3... */
    url += accString;
    url += "&field4="; /* dac1, dac2, dac3... */
    url += dacString;
    url += "&field5=";
    url += String(max_speed);
    url += "&field6=";
    url += String(maxEffortPitchCount);
    
    url += "\r\n";

    // Send request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");


    /* everything in the session should be reset */
    EEPROM.write(0, 0); /* pitchCount*/
    EEPROM.write(2, 0); /* maxEffortPitchCount */
    EEPROM.commit();

    draw_text("Uploaded  data...");
    delay(1000);
    draw_text("Tweeting..");
    if (!client.connect(host, httpPort)) {
      return;
    }
/*
    String tsData("I pitched ");
    tsData += String(pitchCount);
    tsData += String(" times today!!! :)");
    if ( pitchCount > MAX_PITCHES )
      tsData += String(" a little higher than what my mom would recommend.");

    tsData = "api_key=" + String(thingtweetAPIKey) + "&status=" + tsData;

    client.print("POST /apps/thingtweet/1/statuses/update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(tsData.length());
    client.print("\n\n");

    client.print(tsData);
    */
    delay(1000);
    draw_text("Please    reboot!");
    while (1)
      delay(100000);
  }
  else
  {
    int times = 0; //how many times the buffer has been written to the sd-card
    while (1)
    {
      int offset;
      static int v = 0;
      unsigned long last;

      if ( !state )
      {
        count = 0;

        if ( !file_open )
        {
          myFile = SD.open("test.txt", FILE_WRITE);/* overwrite the file */
          file_open = 1;
        }

        state = 1;

      }
      else
      {
        if ( !count )
          //draw_text(String("Pitching.."));
          draw_pitches();
        if ( count == 256 ) // roughly half a sec 1.86ms * 256 = 476ms
        {
          times++;
          count = 0;
          //last = millis();
          myFile.write((const char*)sd_buffer, 2 * 6 * 256);//average sd card write per 256 count 33ms
          //Serial.println(millis() - last);
        }
        if ( times == 4 ) // roughly 2 sec
        {
          myFile.close();
          file_open = 0;

          //Serial.println(String(pitchState));
          //draw_speed(v++);
          draw_pitches();
          state = 0;
          times = 0;
          //delay(500);
          continue;
        }

        s^=1;
        digitalWrite(1,s);
        
        offset = count * 6;

        /* customized raw reading functions */
        /* raw data int16_t for each axis */
        accel.read();
        //mag.read();
        gyro.read();

        sd_buffer[offset + 0] = accel.raw.x;
        sd_buffer[offset + 1] = accel.raw.y;
        sd_buffer[offset + 2] = accel.raw.z;
        sd_buffer[offset + 3] = gyro.raw.x;
        sd_buffer[offset + 4] = gyro.raw.y;
        sd_buffer[offset + 5] = gyro.raw.z;
        //sd_buffer[offset + 6] = mag.raw.x;
        //sd_buffer[offset + 7] = mag.raw.y;
        //sd_buffer[offset + 8] = mag.raw.z;
        count++;
        accel_count++;
        if (absVal(gyro.raw.x) > maxGyroXVal){
          maxGyroXVal = absVal(gyro.raw.x);
          //acc_count = accel_count;
        }
        if (absVal(gyro.raw.y) > maxGyroYVal){
          maxGyroYVal = absVal(gyro.raw.y);
          //acc_count = accel_count;
        }
        if (absVal(gyro.raw.z) > maxGyroZVal){
          maxGyroZVal = absVal(gyro.raw.z);
          acc_count = accel_count;
          
        }

        //prediction code!!
        if (pitchState == 1) {
          
          //0 crossing point
          if(gyro.raw.y <= 0 && !dac_updated && accel_count > 30)
          {
            dac_count = accel_count - acc_count;
            if ( dac_count > 200 )
              dac_count = 1;
            dac_updated = true;
          }
          if (absVal(gyro.raw.x) < DEAD_ZONE_VALUE && absVal(gyro.raw.y) < DEAD_ZONE_VALUE && absVal(gyro.raw.z) < DEAD_ZONE_VALUE) {
            deadZoneCounter++;
    
            //Transition from state 1 to 0
            if (deadZoneCounter == DEAD_ZONE_COUNTER) {
              pitchState = 0;
              deadZoneCounter = 0;
              if (maxGyroXVal > GYRO_INTENSITY_FOR_PITCH || maxGyroYVal > GYRO_INTENSITY_FOR_PITCH || maxGyroZVal > GYRO_INTENSITY_FOR_PITCH) {
                last_speed = (float)maxGyroZVal * 0.00136235223 + 26.66;
                if ( last_speed > max_speed )
                {
                    max_speed = last_speed;
                    EEPROM.write(1, max_speed);
                }
                else if ( last_speed > 0.75 * max_speed )
                {
                    EEPROM.write(2, ++maxEffortPitchCount);
                }
                acc_time = acc_count * 2;
                dac_time = dac_count * 2;
                EEPROM.write(3 + pitchCount * 5, last_speed |  ( game_mode << 7 ) ); 
                EEPROM.write(3 + pitchCount * 5 + 1, acc_time >> 8); 
                EEPROM.write(3 + pitchCount * 5 + 2, acc_time & 0xFF);
                EEPROM.write(3 + pitchCount * 5 + 3, dac_time >> 8); 
                EEPROM.write(3 + pitchCount * 5 + 4, dac_time & 0xFF);

                EEPROM.write(0, ++pitchCount);

                EEPROM.commit();
              }
              maxGyroXVal = 0;
              maxGyroYVal = 0;
              maxGyroZVal = 0;
            }
          }
        }

        else {
          if (absVal(gyro.raw.x) > DEAD_ZONE_VALUE || absVal(gyro.raw.y) > DEAD_ZONE_VALUE || absVal(gyro.raw.z) > DEAD_ZONE_VALUE) {
            eventCounter++;
            //Serial.println();
            //Serial.println("ev++");

            //Transition from state 0 to 1
            if (eventCounter == EVENT_COUNTER) {
              //Serial.println("trans0 - 1");
              pitchState = 1;
              eventCounter = 0;
              accel_count=0;
              dac_updated = false;
            }
          }

        }


        /* _lsm303Accel_MG_LSB:         0.001
          SENSORS_GRAVITY_STANDARD:     9.8
          _lsm303Mag_Gauss_LSB_XY:      1100.0
          _lsm303Mag_Gauss_LSB_Z:       980
          SENSORS_GAUSS_TO_MICROTESLA:  100
          SENSORS_DPS_TO_RADS:          0.017453293
          GYRO_SENSITIVITY_2000DPS:     0.07
        */
        /*Serial.print("aX: "); Serial.print((float)accel.raw.x * _lsm303Accel_MG_LSB * SENSORS_GRAVITY_STANDARD); Serial.print("  ");
          Serial.print("aY: "); Serial.print((float)accel.raw.y * _lsm303Accel_MG_LSB * SENSORS_GRAVITY_STANDARD); Serial.print("  ");
          Serial.print("aZ: "); Serial.print((float)accel.raw.z * _lsm303Accel_MG_LSB * SENSORS_GRAVITY_STANDARD); Serial.print("  ");Serial.println("m/s^2 ");

          Serial.print("mX: "); Serial.print((float)mag.raw.x / _lsm303Mag_Gauss_LSB_XY * SENSORS_GAUSS_TO_MICROTESLA); Serial.print("  ");
          Serial.print("mY: "); Serial.print((float)mag.raw.y / _lsm303Mag_Gauss_LSB_XY * SENSORS_GAUSS_TO_MICROTESLA); Serial.print("  ");
          Serial.print("mZ: "); Serial.print((float)mag.raw.z / _lsm303Mag_Gauss_LSB_Z * SENSORS_GAUSS_TO_MICROTESLA); Serial.print("  ");Serial.println("uT");

          Serial.print("gX: "); Serial.print(gyro.raw.x * SENSORS_DPS_TO_RADS * GYRO_SENSITIVITY_2000DPS); Serial.print("  ");
          Serial.print("gY: "); Serial.print(gyro.raw.y * SENSORS_DPS_TO_RADS * GYRO_SENSITIVITY_2000DPS); Serial.print("  ");
          Serial.print("gZ: "); Serial.print(gyro.raw.z * SENSORS_DPS_TO_RADS * GYRO_SENSITIVITY_2000DPS); Serial.print("  ");Serial.println("rad/s ");
        */



        //delay(MEASUREMENT_DELAY);
      }

    }
  }



}
