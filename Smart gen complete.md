SMS/IOT BASED SMART GENERATOR MONITORING AND CONTROL SYSTEM 

The following are the components used 

1. ESP32 dev module (esp32dev)

2. 20*4 LCD with I2C

4. Sim 800L 

5. Buzzer 

6. MMC Card Reader 

7. DS3231 Clock Module 

8. Voltage Sensors (2Pieces) 

9. Relays (4Pieces) 

10. SG90 Servo Motor 

11. Ultrasonic Sensor 

12. LEDs (4pieces) 

# COMPONENTS PINS
*MMC card reader*
Miso - 19
Mosi - 23
Sck - 18
CS - 5

 *Relay*
Relay2 - 32 (Generator ON/OFF control)
Relay3 - 13 (Battery Charging)
Relay4 - 14 (Load Transfer)

 *Voltage Sensor A (Public Electricity Supply can be shortened to NEPA or PHCN)*
out - 34
 *Voltage Sensor B (Generator Supply)*
out - 35
 *Voltage Divider (Battery voltage)*
out - 15

 *LED*
LED 3 - 4 *SMS Signal*
LED 4 - 2  *Low fuel alert* 

 *Ultrasonic sensor* 
Echo - 26
Trigger - 25

 *Servo*
out - 27

 *Buzzer*
out - 33

 *Sim 800L*
Tx - 16
Rx - 17

Voltage Sensor A (Public Electricity Supply) monitors the Presence of Public Electricity supply, Relay2 would be initiated to turn of the generator and the generator will be in off state, while the system will be monitoring the Public Electricity supply with the help of voltage sensor A, immediately there is absence of Public Electricity supply, the system send an SMS to the registered number indicating that there is absence of power and thereby waiting on a command to ON the Generator. When a ONGEN command is being sent and received by the controller through the Sim800L, The System check the fuel level first, if the fuel level is <30%, the system send an SMS to the registered number indicating the generator cannot be started due low fuel level but if the fuel level is >30% then relay 2 turns on, D9 is connected to the SG90 Servo Motor, this motor serves the function of choking the generator caburetor for a period of 5sec then unchoke it, this operation continues until it detects stable output from the Voltage Sensor B which is responsible for monitoring voltage output from the generator, immediately there is voltage output from the generator it means the generator has started operation the servo motor unchoke completely. Relay 2 remain on until the mains is brought back before turning off. Relay 2 is responsible for bridging the generator power switch together. 

Immediately the generator start operation an SMS is sent indicating that the generator has started operation and a timer function is initiated, the purpose of this timer is to keep record of the number of hours the generator is operating daily, this inform will be save on the 8GB local storage with the help of the MMC Card Reader Module and also on the database with the help of the SIM 800L. The below is typical message sent (JSON format for data sent to the database)

Line 1: Public Electricity Supply Voltage - OFF Line 2: Generator Voltage -230v Line 3: Date and Time Line 4: Fuel Level - 50% 

The SIM 800L serves the function of sending SMS, Receiving SMS and also sending data to the database. 

The buzzer serve as giving alarm/notification at absence or presence of Public Electricity Supply, also when the fuel is low. 

The 20x4 LCD is connected to an I2c, it serves the purpose of displaying the following information. 

1. Welcome message when the system is being powered on 

2. After the system is powered Line 1: Public Electricity Supply Voltage 220V Line 2: Generator Voltage OFF Line 3: Date and Time Line 4: Fuel Level and Possibly Network Status 

The function of the ultrasonic sensor is to read/measure the level of the fuel, this can be display in percentage form on the LCD when the fuel level reach 30%, the alarm beep to indicate the fuel going down and thereby send an SMS to the registered number, when the fuel reach 10%, the alarm beep continuously to indicate the generator is going down soon and also send an SMS again, at this point if a OFFGEN command is send from the registered number the generator will be switch off but if otherwise immediately the fuel level reach 7% the generator is switched off automatically. (The generator fuel tank capacity has to be specified, so i think a variable should hold that value)

The voltage divider serve the purpose of reading voltage of the 12v battery used in powering the system. 

Relay 3 serve the purpose of charging the 12v battery, the battery voltage is less than 11V the relay turns on and thereby start the charging process of the battery but immediately it reaches 13.4V, the relay turns off and thereby stop the charging process of the battery. 

Relay 4 serves as automatically changing the load, i.e during the presence of Public Electricity Supply, load is switch to the normally close of the relay while immediately the generator start working, the relay turns on and thereby switch the load to normally open where the generator live is connected. Immediately the Public Electricity Supply return, the relay 4 automatically switch the load back from generator to Public Electricity Supply but wait for a period of 1minute before switching the generator, i.e turning relay 2 off. 

Led 3 serves as indicating when SMS is being sent or when SMS is being received 

Led 4 serves as indicating low fuel level 

Note: EEPROM function is used in saving variable like 

1. The registered number which can later be change with a command CHPrevious NumberNNew Number I.e CHP09132452423N07034221134 

2. Total No of hours the generator has being operated for the day 

3. Total No of hours the generator has being operated for the period of system operation 

4. Total Fuel used in liters for the day 

5. Total fuel used in liters for the period of system operation 

Note: all these can be reset with a reset command RSTALL or RSTNUMBER

List of Commands and their interpretation 

|**Commands**|**Meaning**|
|---|---|
|ONGEN|Power On the Gen|
|OFFGEN|Power Off the Gen|



CHP09132452423N07034221134 Change previous number to new number 

MSTATUS Request moment status DSTATUS Request the overall day status RSTALL Reset all the variables to their initial values RSTNUMBER Reset the number to the initial register number 

# **Feedback for each Commands** 

**ONGEN:** Immediately the generator start operation i.e there is output at voltage sensor B output, the below message will be send back to the registered number 

Gen:  230v Public Electricity Supply: 0v Date/Time: 20/09/2026 3:30pm Fuel level: 50% 

**OFFGEN:** Immediately the generator is successfully stopped i.e no output at the voltage sensor B output, the below message will be send back to the registered number 

Gen:  0v Public Electricity Supply: depending on main presence or not Date/Time: 20/09/2026 3:30pm Fuel level: depending on the fuel level at the moment 

**CHP09132452423N07034221134:** At a successful number change, the below message will send back to the change number 

Previous Number: **09132452423** New Number: **07034221134** 

**MSTATUS:** This command is used to request for the current System working Status the reply is as follow 

Gen:  depending on the one working Mains: depending on the one working Date/Time: depending on the time Fuel level: depending on the fuel level at the moment 

**DSTATUS:** This command is used to request for the overall daily status of the system 

GEN Working Hrs: No of hours the generator has been operated on that day Fuel Status: I don’t know if it’s possible to convert the value receive from the ultrasonic sensor to liters and be tracking and saving it in order to get the total liters of fuel used at the end of the day 

**RSTALL:** Respond with system successfully reset **RSTNUMBER:** respond with **Previous Number** : **09132452423 Note:** All command can only be send from the saved number if send from any other number no operation will be performed except for **RSTALL** and **RSTNUMBER** that can be send from the initial registered number 

Other Messages from the System 

1. **When the generator wanted to start and the fuel level is below 30** % Generator not Working Reason: Low Fuel Level 20% 2. **When the fuel level reach 30% During generator working moment** Gen:  230v Mains: 0v Date/Time: 20/09/2026 3:30pm Fuel level: 30% 3. **When the fuel level reach 10% During generator working moment** Gen:  230v Mains: 0v Date/Time: 20/09/2026 3:30pm Fuel level: 10% 

4. **When the fuel level below 10% During generator working moment, the generator automatically turn off and then send an SMS** Gen:  0v Mains: 0v Date/Time: 20/09/2026 3:30pm Fuel level: 7% 

# IMPORTANT NOTES
1. the SMS system should only process commands from a default number that would be hardcoded into the code and the phone stored in the EEPROM
2. endpoints should be included so it can be used to build a control dashboard
3. necessary libraries should included into the code