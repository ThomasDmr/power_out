#include "Arduino.h"
#include "Sim800l.h"
#include <SoftwareSerial.h>

namespace Sim800
{

Sim800l::Sim800l(uint8_t pinRX, uint8_t pinTX, uint8_t pinRST, int baudRate = 4800) : m_SIM(pinRX, pinTX), m_pinRST(pinRST), m_baudRate(baudRate)
{
  m_inputTimer = 0;
  m_lastMsgSentLength = 0;
  m_clearLastMessage();
}

//
//PUBLIC METHODS
//

void Sim800l::begin()
{
  // pinMode(m_pinRST, OUTPUT);
  // digitalWrite(m_pinRST, HIGH);
  m_inputBuffer.reserve(50);
  m_SIM.begin(m_baudRate);
  delay(500);
  m_SIM.print(F("AT\r\n"));

  delay(1000);
  m_flush();
}

void Sim800l::reset()
{
  DEBUG_PRINTLN("Reset");
  // digitalWrite(m_pinRST, LOW);
  // delay(1000);
  // digitalWrite(m_pinRST, HIGH);
  // delay(1000);
}

void Sim800l::checkForIncommingData()
{
  if(m_SIM.available())
  {
    m_clearLastMessage();

    char c = m_SIM.read();
    if(c == '\n')
    {
        m_inputBuffer += '/';
    }
    else if(c == '\r' || c == '\t')
    {
        //skip
    }
    else
    {
        m_inputBuffer += c;
    }
    
    m_inputTimer = millis();
  }

  if((m_inputTimer != 0 && millis() - m_inputTimer > 200) || m_inputBuffer.length() > 160)
  {
    m_inputTimer = 0;
    if(m_lastMsgSentLength != 0)
    {
      m_inputBuffer.remove(0, m_lastMsgSentLength - 1);
      m_lastMsgSentLength = 0;
    }
    DEBUG_PRINTLN("Receive:\t" + m_inputBuffer);
    m_lastReceivedMsg = m_inputBuffer;
    m_flush();
  }
}

void Sim800l::sendStringToSIM(String msgToSend)
{
  m_clearLastMessage();
  m_lastMsgSentLength = msgToSend.length();
  DEBUG_PRINTLN("Send:\t" + msgToSend);
  m_SIM.print(msgToSend);
}

int Sim800l::hasCorrectSignal(int callInterval_second)
{
  static uint32_t lastCall = 0;

  if (millis() - lastCall > (uint32_t)callInterval_second * 1000)
  {
    DEBUG_PRINTLN("Connection Check");
    lastCall = millis();
    sendStringToSIM(F("AT+CREG?\r\n"));
    m_waitForResponse(2000);

    int index = m_lastReceivedMsg.indexOf(",");
    if (index != -1)
    {
      int signalValue = (m_lastReceivedMsg.substring(index + 1, index + 2)).toInt();
      if (signalValue == 1 || signalValue == 5 || signalValue == 6  || signalValue == 7)
      {
        m_clearLastMessage();
        return 1;
      }
      else
      {
        m_clearLastMessage();
        return -1;
      }
    }
    else
    {
      m_clearLastMessage();
      return -1;
    }
  }

  return 0;
}

bool Sim800l::newSmsAvailable()
{
  if(m_lastReceivedMsg.length() != 0)
  {
    if (m_lastReceivedMsg.indexOf("+CMTI") != -1)
    {
      m_clearLastMessage();
      DEBUG_PRINTLN("New SMS ! ");
      return true;
    }
    else
    {
      return false;
    }
  }
  return false;
}

bool Sim800l::setToReceptionMode()
{
  bool isOk = true;
  sendStringToSIM("AT+CMGF=1\r\n");
  isOk &= m_messageWellReceived();
  delay(1000);
  sendStringToSIM("AT+CPMS=\"SM\"\r\n");
  isOk &= m_messageWellReceived();
  delay(1000);
  sendStringToSIM("AT+CSDH=1\r\n");
  isOk &= m_messageWellReceived();

  return isOk;
}

bool Sim800l::parseSmsData()
{
  // Send message to retrieve the SMS data
  sendStringToSIM(F("AT+CMGL=\"ALL\"\n\r"));
  m_waitForResponse(2000);

  if(m_lastReceivedMsg.length() == 0)
  {
    return false;
  }
  else
  {
    m_removeCharUntil(","); // remove message type
    m_removeCharUntil(",", 1); // remove "REC UNREAD"
    
    // extract sender's number
    int index = m_lastReceivedMsg.indexOf("\"");
    m_userNumber = m_lastReceivedMsg.substring(0, index);

    if(m_userNumber.length() < 10)
    {
      DEBUG_PRINTLN(F("Err: Msg nbr"));
      m_userNumber = "";
      m_clearLastMessage();
      return false;
    }

    m_removeCharUntil("\"", 1); // remove number
    m_removeCharUntil(",\""); // remove name of sender
    m_removeCharUntil(",\""); // remove date and other data
    m_removeCharUntil(","); // remove extra number
    m_removeCharUntil(","); // remove max data size

    // extract msg length
    index = m_lastReceivedMsg.indexOf("/");
    int msgSize = m_lastReceivedMsg.substring(0, index).toInt();

    if(msgSize == 0 || msgSize > 50)
    {
      DEBUG_PRINTLN(F("Err: Msg Size"));
      m_clearLastMessage();
      return false;
    }

    m_removeCharUntil("/"); // remove max data size
    // extract msg
    m_lastReceivedMsg.remove(msgSize, m_lastReceivedMsg.length());

    return true;
  }
}

bool Sim800l::delAllSms()
{
  DEBUG_PRINTLN("Deleting SMS");
  m_userNumber = "";
  m_userName = "";
  sendStringToSIM(F("AT+CMGD=1,4\n\r"));
  return m_messageWellReceived();
}

msgType Sim800l::extractTypeFromSms()
{
  if(m_lastReceivedMsg == "")
  {
    DEBUG_PRINTLN(F("Err: sms data lost"));
    return ERROR;
  }
  else
  {
    if(m_lastReceivedMsg.indexOf("#Set") != -1)
    {
      DEBUG_PRINTLN("Setting Message");
      if(m_parseSettingsMsg())
      {
        return SETTINGS;
      }
      else
        return ERROR;
    }
    else if(m_lastReceivedMsg.indexOf("#Reset") != -1)
    {
      DEBUG_PRINTLN("Reset Message");
      return RESET;
    }
    else if(m_lastReceivedMsg.indexOf("#Status") != -1)
    {
      DEBUG_PRINTLN("Status Message");
      return STATUS;
    }
    else if(m_lastReceivedMsg.indexOf("#Stop") != -1)
    {
      DEBUG_PRINTLN("Stop Message");
      return STOP;
    }
    else
    {
      return ERROR;
    }
  }
}

bool Sim800l::sendSms(String smsText, String number, String name)
{
  sendStringToSIM("AT+CMGS=\"" + number + "\"\r"); // command to send sms
  m_waitForResponse(2000, false);
  // Send text
  sendStringToSIM("Hi " + name + "\n" + smsText);
  m_SIM.print((char)26);
  m_waitForResponse(2000);
  //expect CMGS:xxx   , where xxx is a number,for the sending sms.
  if (((m_lastReceivedMsg.indexOf("CMGS")) != -1))
  {
    m_clearLastMessage();
    return true;
  }
  else
  {
    m_clearLastMessage();
    m_waitForResponse(6000);
    if (((m_lastReceivedMsg.indexOf("CMGS")) != -1))
    {
      m_clearLastMessage();
      return true;
    }
    else
    {
      DEBUG_PRINTLN(m_lastReceivedMsg);
      DEBUG_PRINTLN(F("Err: SMS send errror"));
      m_clearLastMessage();
      return false;
    }
  }
}

String Sim800l::getUserName()
{
  return m_userName;
}

String Sim800l::getUserNumber()
{
  return m_userNumber;
}
//
//PRIVATE METHODS
//

void Sim800l::m_flush()
{
  while (m_SIM.available())
  {
    byte junk = m_SIM.read();
  }
  m_inputBuffer = "";
}

bool 	Sim800l::m_messageWellReceived()
{
  m_waitForResponse(2000);

  if (m_lastReceivedMsg.indexOf("OK") != -1)
  {
    m_clearLastMessage();
    return true;
  }
  else
  {
    m_clearLastMessage();
    return false;
  }
}

void Sim800l::m_waitForResponse(int timeOut, bool keepMessage)
{
  uint32_t timer = millis();
  while(m_lastReceivedMsg.length() == 0 && millis() - timer < timeOut)
  {
    checkForIncommingData();
  }

  if(!keepMessage)
  {
    m_clearLastMessage();
  }
}

void 	Sim800l::m_removeCharUntil(String character, int extraChar)
{
  int index = m_lastReceivedMsg.indexOf(character);
  m_lastReceivedMsg.remove(0, index + 1 + extraChar);
}

bool Sim800l::m_parseSettingsMsg()
{
  int nameIdx = m_lastReceivedMsg.indexOf("#Name:");
  int nbrIdx = m_lastReceivedMsg.indexOf("#Nbr:");
  
  if(nameIdx != -1 && nbrIdx != -1)
  {
    int nameEndIndex = m_lastReceivedMsg.indexOf("#", nameIdx + 1);
    int nbrEndIndex = m_lastReceivedMsg.indexOf("#", nbrIdx + 1);

    if(nameEndIndex == -1 || nbrEndIndex == -1 || nbrEndIndex == nameIdx || nameEndIndex == nbrIdx)
    {
      m_userNumber = "";
      m_userName = "";
      m_clearLastMessage();
      DEBUG_PRINTLN(F("Err: incorrect settings"));
      return false;
    }
    else
    {
      m_userName = m_lastReceivedMsg.substring(nameIdx + 6, nameEndIndex);

      if(m_lastReceivedMsg.indexOf("this", nbrIdx) == -1)
      {
        m_userNumber = m_lastReceivedMsg.substring(nbrIdx + 5, nbrEndIndex);

        if(m_userNumber.length() < 10)
        {
          m_userNumber = "";
          m_userName = "";
          m_clearLastMessage();
          DEBUG_PRINTLN(F("Err: incorrect number"));
          return false;
        }
      }

      m_clearLastMessage();
      DEBUG_PRINTLN(String(m_userName) + "\t" + String(m_userNumber));
      return true;
    }
  }
  else
  {
    m_userNumber = "";
    m_userName = "";
    m_clearLastMessage();
    DEBUG_PRINTLN(F("Err: incorrect settings"));
    return false;
  }
}

void Sim800l::m_clearLastMessage()
{
  m_lastReceivedMsg = "";
}

} //namespace Sim800