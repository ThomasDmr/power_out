#include "Sim800l.h"
#include <EEPROM.h>
#include "config.h"

Sim800::Sim800l sim800l(RX_PIN, TX_PIN, RESET_PIN, 4800); 

int  normalInputVoltage = 0;
uint32_t lastMessageSent = 0;
bool warningMessageSent = false;
bool powerBackMessageSent = true;

String userName = "";
String userNumber = "";

byte pwrControlByte = 0;

void setup()
{
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);

  pinMode(MUX_STATUS, INPUT);
  pinMode(CHARGE_STATUS, INPUT);

  // Power is on battery by default in case the power swithing 
  // didn't go as planned and the board rebooted
  pinMode(PWR_SELECT, OUTPUT);

  if(isPowerConnected())
    digitalWrite(PWR_SELECT, HIGH);
  else
    digitalWrite(PWR_SELECT, LOW);

  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);

  Serial.begin(115200);
  bitSet(pwrControlByte, PWR_SELECT);

  sim800l.begin(); // initializate the library.

  digitalWrite(LED_R, LOW);

  initConnection();

  digitalWrite(LED_R, HIGH);

  while(!sim800l.setToReceptionMode())
  {
    blinkLed(LED_G, 500, 0);
    delay(500);
  }

  
  userName = getUserName();
  userNumber = getUserNumber();
  warningMessageSent = getWarningSent();
  powerBackMessageSent = getPowerBackSent();

  DEBUG_PRINTLN(userName + " " + userNumber);
  DEBUG_PRINTLN(String(warningMessageSent) + " " + String(powerBackMessageSent));

  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);

  normalInputVoltage = getBandgap();
}

void loop(){

  static uint32_t timer = millis();

  selectPowerInput(normalInputVoltage);

  getMessageFromSerial();
  sim800l.checkForIncommingData();

  if(gsmSignalOK())
  {
    if(isUserConfigurated())
    {
      if(!isPowerConnected())
      {
        digitalWrite(LED_G, LOW);
        slowBlink();

        if(powerBackMessageSent)
        {
          powerBackMessageSent = false;
          storePowerBackSent(false);
        }

        if(!warningMessageSent && (lastMessageSent == 0 || millis() - lastMessageSent > 30000))
        {
          delay(1000);
          sim800l.setToReceptionMode();
          storeWarningSent(true);
          if(sim800l.sendSms(F("Watch out ! Your power went down."), userNumber, userName))
          {
            lastMessageSent = 0;
            warningMessageSent = true;
          }
          else
          {
            storeWarningSent(false);
            lastMessageSent = millis();
          }
        }
      }
      else
      {
        digitalWrite(LED_R, !isBatteryCharged());
        digitalWrite(LED_G, HIGH);

        if(warningMessageSent)
        {
          warningMessageSent = false;
          storeWarningSent(false);
        }

        if(!powerBackMessageSent && (lastMessageSent == 0 || millis() - lastMessageSent > 120000))
        {
          delay(1000);
          sim800l.setToReceptionMode();
          storePowerBackSent(true);
          if(sim800l.sendSms(F("Good news ! It looks like your power is back on."), userNumber, userName))
          {
            lastMessageSent = 0;
            powerBackMessageSent = true;
          }
          else
          {
            storePowerBackSent(false);
            lastMessageSent = millis();
          }
        }
      }
    }
    else
    {
      blinkLed(LED_R, 500, LED_G);
    }

    if(sim800l.newSmsAvailable())
    {
      for(int i=0; i < 5; i++)
      {
        digitalWrite(LED_G, (i+1)%2);
        delay(50);
      }

      if(sim800l.parseSmsData())
      {
        Sim800::msgType receivedMsgType = sim800l.extractTypeFromSms();
        
        if(receivedMsgType == Sim800::ERROR)
        {
          // Ignore
        }
        else if(receivedMsgType == Sim800::RESET)
        {
          userName = "";
          userNumber = "";
          clearUserInformation();
          storeWarningSent(0);
          storePowerBackSent(1);
        }
        else if(receivedMsgType == Sim800::SETTINGS)
        {
          userNumber = sim800l.getUserNumber();
          userName = sim800l.getUserName();

          storeUserName(userName);
          storeUserNumber(userNumber);

          sim800l.sendSms(F("Welcome to you!"), userNumber, userName);
        }
        else if(receivedMsgType == Sim800::STATUS)
        {
          if(isUserConfigurated())
          {
            if(isPowerConnected())
              sim800l.sendSms(F("Everything is working fine."), userNumber, userName);
            else
              sim800l.sendSms(F("Oh oh ! You seem to have lost power..."), userNumber, userName);
          }
        }
        else if(receivedMsgType == Sim800::STOP)
        {
          if(isPowerConnected())
          {
            powerBackMessageSent = true;
            warningMessageSent = false;
            storeWarningSent(0);
            storePowerBackSent(1);
          }
          else
          {
            powerBackMessageSent = false;
            warningMessageSent = true;
            storeWarningSent(1);
            storePowerBackSent(0);
          }
        }

        sim800l.delAllSms();
      }
      else
      {
        sim800l.delAllSms();
      }
    }
  }
  else
  {
    blinkLed(LED_R, 500, 0);
    digitalWrite(LED_G, LOW);
  }
}

bool selectPowerInput(int& initVoltage)
{
  static bool switched = true;
  int supplyVoltage = getBandgap();

  if(supplyVoltage > initVoltage)
  {
    initVoltage = supplyVoltage;
  }

  if(!switched && supplyVoltage < (long)initVoltage * 92 / 100)
  {
    // switch to battery input
    PORTD &= !pwrControlByte;  // set pin to low
    switched = true;
    DEBUG_PRINTLN("BAT IN");
    DEBUG_PRINTLN(String(supplyVoltage) + "\t" + String(initVoltage));
  }
  else if(switched && isPowerConnected())
  {
    PORTD |= pwrControlByte; // set pin to high
    switched = false;
    DEBUG_PRINTLN("USB IN");
    DEBUG_PRINTLN(String(supplyVoltage) + "\t" + String(initVoltage));
  }

  /*
  static uint32_t timer = millis();
  if(millis() -  timer > 300)
  {
    DEBUG_PRINTLN(String(supplyVoltage) + "\t" + String(initVoltage));
  }
  */

  return switched;
}

void getMessageFromSerial()
{
  static String buffer = "";
  static uint32_t timer = 0;
  
  if(Serial.available())
  {
    char c = Serial.read();
    buffer += c;
    timer = millis();
  }

  if(timer != 0 && millis() - timer > 100)
  {
    timer = 0;
    sim800l.sendStringToSIM(buffer);
    while (Serial.available())
    {
      byte junk = Serial.read();
    }
    buffer = "";
  }
}

void initConnection()
{
  bool isConnected = false;

  uint32_t initTimer = 0;
  uint32_t globalTimer = millis();
  bool ledON = true;

  while(!isConnected)
  {
    if(sim800l.hasCorrectSignal(2) == 1)
    {
      if(initTimer == 0)
      {
        initTimer = millis();
      }
      else if(millis() - initTimer > 60000L)
      {
        isConnected = true;
      }
    }
    else if(sim800l.hasCorrectSignal(2) == -1)
    {
      initTimer = 0;
    }

    if(initTimer == 0 && millis() - globalTimer > 30000)
    {
      //sim800l.reset();
      globalTimer = millis();
    }

    blinkLed(LED_R, 500, 0);
  }
  DEBUG_PRINTLN("Connected !");
}

bool gsmSignalOK()
{
  static bool gmsSignal = true;
  static int interval = 20;
  
  int signalStatus = sim800l.hasCorrectSignal(interval);

  if(signalStatus == 1)
  {
    interval = 20;
    gmsSignal = true;
  }
  else if(signalStatus == -1)
  {
    interval = 5;
    gmsSignal = false;
  }

  return gmsSignal;
}

bool isBatteryCharged()
{
  return digitalRead(CHARGE_STATUS);
}

void blinkLed(int ledPin, int interval, int extraLedPin)
{
  static uint32_t timer = 0;
  static bool     state = 1;
  if(millis() - timer > interval)
  {
    digitalWrite(ledPin, state);
    if(extraLedPin != 0) 
      digitalWrite(extraLedPin, state);
    state = !state; 
    timer = millis();
  }
}

void slowBlink()
{
  static int32_t timer = millis();
  if(millis() - timer > 2000)
  {
    digitalWrite(LED_R, HIGH);
    delay(50);
    digitalWrite(LED_R, LOW);
    timer = millis();
  }
}

bool isUserConfigurated()
{
  return userName.length() != 0 && userNumber.length() != 0;
}

void storeUserName(String userName)
{
  uint8_t nameLength = min(userName.length(), 20);

  EEPROM.write(5, nameLength); 
  for(int i=0; i<nameLength; i++)
  {
    EEPROM.write(10+i, (byte)userName[i]);
  }
}

void storeWarningSent(bool warningSent)
{
  EEPROM.write(1, warningSent); 
}

void storePowerBackSent(bool powerBack)
{
  EEPROM.write(2, powerBack); 
}

bool getWarningSent()
{
  uint8_t value = EEPROM.read(1);
  return (value == 1);
}

bool getPowerBackSent()
{
  uint8_t value = EEPROM.read(2);
  return (value == 1);
}

void storeUserNumber(String userNumber)
{
  uint8_t numberLength = min(userNumber.length(), 20);
  
  EEPROM.write(6, numberLength); 
  for(int i=0; i<numberLength; i++)
  {
    EEPROM.write(30+i, (byte)userNumber[i]);
  }
}

void clearUserInformation()
{
  EEPROM.write(5, 0);
  EEPROM.write(6, 0); 
}

String getUserNumber()
{
  uint8_t numberLength = EEPROM.read(6);
  
  if(numberLength == 0 || numberLength > 20)
  {
    return "";
  }
  
  String number = "";
  for(int i=0; i<numberLength; i++)
  {
    number += (char)EEPROM.read(30+i);
  }

  return number;
}

String getUserName()
{
  uint8_t nameLength = EEPROM.read(5);
  
  if(nameLength == 0 || nameLength > 20)
  {
    return "";
  }
  
  String name = "";
  for(int i=0; i<nameLength; i++)
  {
    name += (char)EEPROM.read(10+i);
  }

  return name;
}

bool isPowerConnected()
{
  return analogRead(PWR_FEEDBACK) > 950;
}


int getBandgap(void) // Returns actual value of Vcc (x 100)
{
  // For 168/328 boards
  const long InternalReferenceVoltage = 1056L;  // Adjust this value to your boards specific internal BG voltage x1000
  // REFS1 REFS0          --> 0 1, AVcc internal ref. -Selects AVcc external reference
  // MUX3 MUX2 MUX1 MUX0  --> 1110 1.1V (VBG)         -Selects channel 14, bandgap voltage, to measure
  ADMUX = (0 << REFS1) | (1 << REFS0) | (0 << ADLAR) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1) | (0 << MUX0);

  delay(50);  // Let mux settle a little to get a more stable A/D conversion
  // Start a conversion
  ADCSRA |= _BV( ADSC );
  // Wait for it to complete
  while ( ( (ADCSRA & (1 << ADSC)) != 0 ) );
  // Scale the value
  int results = (((InternalReferenceVoltage * 1024L) / ADC) + 5L) / 10L; // calculates for straight line value
  return results;
}

/*
Configuration messages :
#Set
#Name:UserName#
#Nbr:+33XXXXXXXXXX#   --> or #Nbr:this#
*/
