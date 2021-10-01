# Overview
This is an Ardunino Sketch for ESP8266 that receives the current time from NTP server and outputs it to serial interface. ESP8266 module installed this sketch is used as a time source for low performance devices, e.g. Microchip PIC micro conroller. Actually, the current serial output format is adjusted to RTC module of PIC. And this sketch outputs the time info by the request from the client device, and with the timing so that the client device can receive the info just when the second of time info is changed. So, the client device can set the time info to RTC when it receives.

# Sketch Build Environment
- [Ardunino IDE](https://www.arduino.cc/en/software)
- [Borad Manager for ESP8266](http://arduino.esp8266.com/stable/package_esp8266com_index.json)

# How to use
1. After upload image to ESP8266 module, connect PC to ESP8266 module with serial port.<br>
   ***Terminal Setting***<br>
   New Line Rx: CR+LF<br>
   New Line Tx: CR<br>
   ***Serial Setting***<br>
   Board rate: 115200<br>
   Data: 8bit<br>
   Parity: none<br>
   Stop bit: 1 bit<br>
   Flow Control: none<br>
3. Send the following command to ESP8266 module to connect to WiFi AP.<br>
   `wifibegin=SSID,password<CR>`
5. Send the following command to ESP8266 module to get the current time.<br>
   `time=NTP server name<CR>`<br>
   <br>
   ***Recorded terminal logs with Teraterm***<br>
   ```
   [Fri Oct 01 16:13:17.977 2021] OK_21-10-01-5T16:13:18
   [Fri Oct 01 16:13:22.983 2021] OK_21-10-01-5T16:13:23
   [Fri Oct 01 16:13:30.984 2021] OK_21-10-01-5T16:13:31
   ```
   Before captureing the log, PC time is adjusted to ntp.nict.jop. And we got the time from it. The timestamp in the log (inside the blankets) are PC time. We can see the error between the log receive time and log time is with several milli seconds.
   
# Tips
This implemtation decides the log send timing by taking acount of SNTP packet trasmission delay, internal process delay, and serial comunication delay of sending time info to the client device. If you change serial boad rate and/or output time format, you should adjust the delay calculation code in get_time function. 

# Verified Board Configuration
I connected ESP-WROOM-02 module and USB-serial converter on bread board. The connection diagram is the following.
![ESP-WROOM-02-connection](https://user-images.githubusercontent.com/91722851/135625086-2e584b10-0ff6-483a-8891-57860a88ada3.png)

- [Akiduki Denshi Wi-Fi module ESP-WROOM-02 DIP kit](https://akizukidenshi.com/catalog/g/gK-09758/)
- [Akiduki Denshi FT231X USB-Serial conversion module](https://akizukidenshi.com/catalog/g/gK-06894/)

