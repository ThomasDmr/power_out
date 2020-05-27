#include <Sim800l.h>
#include <EEPROM.h>

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINTLN(x)
#endif

#define LED_R A0
#define LED_G A1
#define PWR_FEEDBACK A2
#define RX_PIN 2
#define TX_PIN 3	
#define RESET_PIN 4  

Sim800l sim800l(RX_PIN, TX_PIN, RESET_PIN, 4800); 

void setup()
{
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);

  Serial.begin(115200);
  
  digitalWrite(LED_R, 0);

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
	//do nothing
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
        bool test = sim800l.delAllSms();
        if(!test)
        {
          sim800l.delAllSms();
        }
      }
      else
      {
        sim800l.delAllSms();
        DEBUG_PRINTLN("Error parsing sms!");
      }
    }

    if(sim800l.isUserConfigurated())
    {
      if(hasPowerCutOff())
      {
        sim800l.sendWarning();
      }
      else
      {
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
    digitalWrite(LED_G, LOW);
  }

  if(sim800l.newUserConf())
  {
    DEBUG_PRINTLN("Writing to EEPROM");
    storeUserName(sim800l.userName);
    storeUserNumber(sim800l.userNumber);
  }

  /* static uint32_t tt = millis();
  if(millis() - tt > 1000)
  {
    Serial.println(analogRead(PWR_FEEDBACK));
    tt = millis();
  } */
}


void initConnection()
{
  bool isConnected = false;

  uint32_t initTimer = 0;

  while(!isConnected)
  {
    if(sim800l.hasCorrectSignal(1))
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

  EEPROM.write(1, nameLength); 
  for(int i=0; i<nameLength; i++)
  {
    EEPROM.write(10+i, (byte)userName[i]);
  }
}

void storeUserNumber(String userNumber)
{
  uint8_t numberLength = min(userNumber.length(), 20);
  
  EEPROM.write(2, numberLength); 
  for(int i=0; i<numberLength; i++)
  {
    EEPROM.write(30+i, (byte)userNumber[i]);
  }
}

String getUserNumber()
{
  uint8_t numberLength = EEPROM.read(2);
  
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
  uint8_t nameLength = EEPROM.read(1);
  
  if(nameLength == 0 || nameLength > 20)
  {
    return "Thomas";
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

/*
Configuration messages :
#Set
#Name:UserName#
#Nbr:+33XXXXXXXXXX#   --> or #Nbr:this#
*/