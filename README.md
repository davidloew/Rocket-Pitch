Rokect Pitch
============

**University of Pennsylvania, ESE 519: Real Time and Embedded Systems**

* Tianyang Chen
* Rishab Gupta
* Namneet Kaur
* [Blog]https://devpost.com/software/rocket-pitch

### DESCRIPTION AND GOALS
Teenager baseball player tend to suffer from albow and arm injures from pitching too many times in consecutive days. Many young players terminated their career due to injures. Many research has recommanded some rest days in between plays for couches as well as parents to protect their kids.

This is the repo for the Baseball pitch detection project for teenager players.
The goal of this device is to accurately detect pitch events and give an estimated speed to a user wearing the device. The raw data is logged onto the SD card for future research and algorithm tuning.

The data logged include accelerometer and gyroscope in all three axis. The logging rate is around 537 Hz. Every half a second, the buffered data will be flushed to the SD card to ensure data persistency.

### VIDEO
https://www.youtube.com/watch?v=B9WVGBNP6yY

### IMAGES and GIFs
The final device only has the OLED screen exposed to users. With the help of a 100mAh Li-po battery, the device is much lighter and flatter.

![](https://challengepost-s3-challengepost.netdna-ssl.com/photos/production/software_photos/000/453/680/datas/gallery.jpg)
![](https://challengepost-s3-challengepost.netdna-ssl.com/photos/production/software_photos/000/453/318/datas/gallery.jpg)


### BUILD INSTRUCTIONS
The project is using ESP8266 miro-controller, Adafruit 9dof IMU, Adafruit OLED, micro-SD card.
Clone the repo and import the library folders to Arduino. Download the wifi manager library for ESP8266. The modified libraries have extra low level functions that are not available in the original adafruit libraries. 

### HARWARE SETUP
![](images/Sys.png)

Connections:

| ESP       | MicroSD breakout |
| ------------- |-------------|
| D14      | SCK |
| D12      | MISO |
| GND | GND |
     
| ESP       | Adafruit 9dof    |
| ------------- |-------------|
| D5      | SCL |
| D4      | SDA      |
| GND | GND      |

| ESP       | Adafruit OLED    |
| ------------- |-------------|
| D5      | SCL |
| D4      | SDA      |
| GND | GND      |

| ESP       | push button    |
| ------------- |-------------|
| D2      | GND |
 
The power button should be connected between the power connector and the VCC pin.

### Software Implementations:
The system communicates with all the sensors at around 537Hz maximium. In order to ensure the data consistency, evey second of samples are written to the SD card. The buffered write process takes around 30ms, which means only one sample is lost. This is acceptable because we can just intropolate the missing sample. During huge acceleration, it's wise to write to the backing storage once a while since the connection of the SD card might become loose. The samples are saved into the memory in this pattern: accX, accY, accZ, gyroX, gyroY, gyroZ with two bytes each degree of freedom. Every 1024 samples, data will be flushed to the SD card for persistency and future research use. Some visualisation scripts were written in MATLAB to calibrate the sensors.

Modes WiFi: In the wifi upload mode, the user needs to press a button during the initialization and it will start a hotspot with a webserver. Users now can connect to it and navigate to 192.168.4.1 to see the user interface in which it can be configured to connect their own home wifi. Once the SSID and password are saved, it will try to connect to the same hotspot in the future. If, the same hotspot is not found, the system will go into the configuration mode again and connect to another wifi hotspot.

Game Mode: This mode keeps track of the number of pitches done while a player is pitching in a game as opposed to pitching in practice/ recess. This knowledge is quite crucial for the doctors since children usually over-exert themselves during games. This information is encoded in the first bit of the speed.

Raw DATA format:

The size of the EEPROM is 512B. It is used to store pitch information so data can be uploaded later.

| Variable       | Length    | Notes      |
| ------------- |-------------|-------------|
|pitchCount | 1B | session pitch count, can be reset when holding button down for 6 seconds |
|maxVever | 1B | max speed ever for this user. will not reset. init val 0 |
|maxEffort | 1B | session max effort pitch count. can be reset when holding button down for 6 seconds |
|v1| 1B|speed for pitch 1|
|a1| 2B|acceleration time for pitch 1|
|d1| 2B|deacceleration time for pitch 1|
|v1| 1B|
|a1| 2B|
|d1| 2B|

The software on startup will read the max speed for the current user, and pitchCount so it will not overwrite the old data. Everything will be reset except the max speed when uploading to the cloud. 

For the SD card, the big endian raw data format is:

| Variable       | Length    | Notes      |
| ------------- |-------------|-------------|
|accx | 2B | x direction acc |
|accy | 2B | y direction acc |
|accz | 2B | z direction acc |
|gryx | 2B | x direction gry |
|gry | 2B | y direction gry |
|gryz | 2B | z direction gry |

### System Limitations:
The 9dof sensor from Adafruit has some limitations. The accelerometer has a range of +-20G and the gyroscope can detect maximum 2000dps. While they might be good for normal hobbiests projects, they are not adequte for baseball pitching speed prediction. During a normal pitch, most axis on both the gyro and the accelerometer are capping out. Therefore, we suggest the next version to use better IMUs. So far we have found an analogue accelerometer from Adafruit which has a range of +-200G. However, for the gyroscope, even though there are many chips available, there are not many breakout boards that we can buy.
