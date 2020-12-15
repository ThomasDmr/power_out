#include <Sim800l.h>
#include <EEPROM.h>

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINTLN(x)
#endif

#define LED_R A0
#define LED_G A1
#define PWR_FEEDBACK A2
#define LIPO_FEEDBACK A3
#define TX_PIN 7
#define RX_PIN 8	
#define RESET_PIN 6  

Sim800l sim800l(RX_PIN, TX_PIN, RESET_PIN, 4800); 

bool powerWasCut = false;

void setup()
{
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);

  Serial.begin(115200);
  
  for(int i = 0; i<6; i++)
  {
    digitalWrite(LED_R, i%2);
    digitalWrite(LED_G, i%2);
    delay(300);
  }

  DEBUG_PRINTLN("Hello !");

  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);

	sim800l.begin(); // initializate the library. 
  delay(2000);

  initConnection();
 
  while(!sim800l.setToReceptionMode())
  {
    delay(1000);
  }

  sim800l.userName = getUserName();
  sim800l.userNumber = getUserNumber();

  DEBUG_PRINTLN(sim800l.userName);
  DEBUG_PRINTLN(sim800l.userNumber);

  digitalWrite(LED_R, 1);
}

void loop(){

  int charge = checkLiPoCharge();
  sim800l.setLipoValue(charge);

  if(sim800l.hasCorrectSignal(60))
  {
    digitalWrite(LED_R, HIGH);
    if(sim800l.checkIfNewSMS())
    {
      bool worked = sim800l.parseSmsData();

      if(worked)
      {
        DEBUG_PRINTLN("Worked !");
        sim800l.extractSettings();
      }
      else
      {
        sim800l.delAllSms();
        DEBUG_PRINTLN("Error parsing sms!");
      }

      bool deletionOK = sim800l.delAllSms();
      if(!deletionOK)
      {
        // Retry
        sim800l.delAllSms();
      }
    }

    if(sim800l.isUserConfigurated())
    {
      if(hasPowerCutOff())
      {
        sim800l.setPowerSource(false);

        if(sim800l.sendWarning())
        {
          powerWasCut = true;
        }
        else if(!powerWasCut)
        {
          DEBUG_PRINTLN("error sending Warning");
          sim800l.firstCall = false; // retry sending sms
        }
      }
      else if(powerWasCut)
      {
        sim800l.setPowerSource(true);
        powerWasCut = false;
        sim800l.sendPowerBack();
      }
      else 
      {
        sim800l.setPowerSource(true);
        sim800l.firstCall = true;
      }
    }
  }
  else
  {
    digitalWrite(LED_R, LOW);
    sim800l.checkIfNewSMS();
  }

  sim800l.updateSerial();


  if(!sim800l.isUserConfigurated())
  {
    static int32_t timer = millis();
    static bool val = true;
    if(millis() - timer > 1000)
    {
      digitalWrite(LED_G, val);
      timer = millis();
      val=!val;
    } 
  }
  else
  {
    if(powerWasCut)
    {
      digitalWrite(LED_G, HIGH);
      static int32_t timer = millis();
      if(millis() - timer > 4000)
      {
        digitalWrite(LED_R, LOW);
        delay(500);
        digitalWrite(LED_R, HIGH);
        timer = millis();
      }
    }
    else
      digitalWrite(LED_G, LOW);
  }

  if(sim800l.newUserConf())
  {
    DEBUG_PRINTLN("Writing to EEPROM");
    storeUserName(sim800l.userName);
    storeUserNumber(sim800l.userNumber);
  }
}


void initConnection()
{
  bool isConnected = false;

  uint32_t initTimer = 0;

  while(!isConnected)
  {
    if(sim800l.hasCorrectSignal(2))
    {
      if(initTimer == 0)
      {
        initTimer = millis();
      }
      else if(millis() - initTimer > 5000)
      {
        isConnected = true;
      }
    }
    else
    {
      initTimer = 0;
    }
  }
  DEBUG_PRINTLN("Connected !");
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

void storeUserNumber(String userNumber)
{
  uint8_t numberLength = min(userNumber.length(), 20);
  
  EEPROM.write(6, numberLength); 
  for(int i=0; i<numberLength; i++)
  {
    EEPROM.write(30+i, (byte)userNumber[i]);
  }
}

String getUserNumber()
{
  delay(500);
  uint8_t numberLength = EEPROM.read(6);
  
  if(numberLength == 0 || numberLength > 20)
  {
    return "+33000000000";
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
  delay(500);
  uint8_t nameLength = EEPROM.read(5);
  
  if(nameLength == 0 || nameLength > 20)
  {
    return "Default";
  }
  
  String name = "";
  for(int i=0; i<nameLength; i++)
  {
    name += (char)EEPROM.read(10+i);
  }

  return name;
}

bool hasPowerCutOff()
{
  return analogRead(PWR_FEEDBACK) < 300;
}

int checkLiPoCharge()
{
  int input = analogRead(LIPO_FEEDBACK);

  if(input < 100)
  {
    return -1;
  }
  else if(input < 740)
  {
    return 0;
  }
  else
  {
    return min(input - 740, 100);
  }
}

/*
Configuration messages :
#Set
#Name:UserName#
#Nbr:+33XXXXXXXXXX#   --> or #Nbr:this#
*/
