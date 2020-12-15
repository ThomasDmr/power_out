#include "Arduino.h"
#include "Sim800l.h"
#include <SoftwareSerial.h>

Sim800l::Sim800l(uint8_t pinRX, uint8_t pinTX, uint8_t pinRST, int baudRate = 4800) : _SIM(pinRX, pinTX), _pinRST(pinRST), _baudRate(baudRate)
{
  userName = "Default";
  userNumber = "+33000000000";
  _newUser = false;
  _timeInterval = 0;
  firstCall = true;
  _firstAccuWarning = true;
}

//
//PUBLIC METHODS
//

void Sim800l::begin()
{
  _buffer.reserve(50);
  _SIM.begin(_baudRate);
  delay(500);
  _SIM.print(F("AT\r\n"));

  delay(1000);
  _flush();
}

bool Sim800l::setToReceptionMode()
{
  bool isOk = true;
  _SIM.print("AT+CMGF=1\r\n");
  isOk &= _msgRecieved();
  _SIM.print("AT+CPMS=\"SM\"\r\n");
  isOk &= _msgRecieved();
  _SIM.print("AT+CSDH=1\r\n");
  isOk &= _msgRecieved();

  return isOk;
}

void Sim800l::setLipoValue(int charge)
{
  _charge = charge;

  if(_charge != -1 && _charge <= 15 && _firstAccuWarning)
  {
    _sendSms("Watch out, the accu level is low !");
    _firstAccuWarning = false;
  }
}

void Sim800l::reset()
{
  digitalWrite(_pinRST, 1);
  delay(1000);
  digitalWrite(_pinRST, 0);
  delay(1000);
}

bool Sim800l::isUserConfigurated()
{
  if(userNumber == "+33000000000" || userNumber[0] != '+' || userNumber.length() < 12 || userNumber.length() > 15)
  {
    return false;
  }
  else if(userName == "Default" || userName == "" || userNumber.length() < 1)
  {
    return false;
  }

  return true;
}

bool Sim800l::hasCorrectSignal(int callInterval_second)
{
  static uint32_t lastCall = 0;
  static bool lastValue = false;

  if (millis() - lastCall > callInterval_second * 1000)
  {
    DEBUG_PRINTLN("Connection Check");
    lastCall = millis();
    _SIM.print(F("AT+CREG?\r\n"));
    delay(2000);
    _buffer = _readSerialString();
    DEBUG_PRINTLN(_buffer);
    int index = _buffer.indexOf(",");
    if (index != -1)
    {
      int signalValue = (_buffer.substring(index + 1, index + 2)).toInt();
      if (signalValue == 1 || signalValue == 5 || signalValue == 6  || signalValue == 7)
      {
        lastValue = true;
        _buffer = "";
        return true;
      }
      else
      {
        lastValue = false;
        _buffer = "";
        return false;
      }
    }
    else
    {
      lastValue = false;
      _buffer = "";
      return false;
    }
  }

  return lastValue;
}

bool Sim800l::checkIfNewSMS()
{
  if(_SIM.available())
  {
    _buffer = _readSerialString();

    DEBUG_PRINTLN(_buffer);

    if (_buffer.indexOf("+CMTI") != -1)
    {
      _buffer = "";
      DEBUG_PRINTLN("New SMS ! ");
      return true;
    }
    else
    {
      _buffer = "";
      return false;
    }
  }

  return false;
}

bool Sim800l::_sendSms(String text)
{
  _SIM.print(F("AT+CMGS=\"")); // command to send sms
  _SIM.print(userNumber);
  _SIM.print(F("\"\r"));
  delay(3000);
  // Send text
  _buffer = _readSerialString();
  _SIM.print(text);
  _SIM.print("\r\n");
  delay(3000);
  // Add lipo charge information
  _buffer = _readSerialString();
  if(_charge == -1)
  {
    _SIM.print("!! No accu connected !!");
  }
  else
  {
    _SIM.print("Accu level : " + String(_charge) + "%");
  }
  delay(3000);
  _buffer = _readSerialString();
  _SIM.print((char)26);
  delay(6000);
  _buffer = _readSerialString();
  //expect CMGS:xxx   , where xxx is a number,for the sending sms.
  if (((_buffer.indexOf("CMGS")) != -1))
  {
    _buffer = "";
    return true;
  }
  else
  {
    DEBUG_PRINTLN("not ok");
    _buffer = "";
    return false;
  }
}

void Sim800l::setTimeInterval(uint16_t timeInMinutes)
{
  _timeInterval = (uint32_t)timeInMinutes * 60 * 1000;
}

bool Sim800l::sendWarning()
{
  bool msgSent  = false;
  static uint32_t timer = millis();

  if(firstCall || (_timeInterval != 0 && millis() - timer > _timeInterval))
  {
    DEBUG_PRINTLN("Sending Warning");
    msgSent = _sendSms("Watch out " + String(userName) + ", your power dropped out !");
    if(msgSent)
    {
      DEBUG_PRINTLN("Warning Sent");
      timer = millis();
      firstCall = false;
    }
  }

  return msgSent;
}

bool Sim800l::sendPowerBack()
{
    DEBUG_PRINTLN("Sending Power Back");
    _sendSms("Hi " + String(userName) + ", your power is back on!");
}

bool Sim800l::newUserConf()
{
  bool tmp = _newUser;
  _newUser = false;
  return tmp;
}

bool Sim800l::delAllSms()
{
  DEBUG_PRINTLN("Deleting SMS");
  _SIM.print(F("AT+CMGD=1,4\n\r"));
  return _msgRecieved();
}

void Sim800l::updateSerial()
{
  delay(50);
  while (Serial.available())
  {
    _SIM.write(Serial.read()); //Forward what Serial received to Software Serial Port
  }
}

bool Sim800l::parseSmsData()
{
  const char initMsg[13] = {'A', 'T', '+', 'C', 'M', 'G', 'L', '=', '"', 'A', 'L', 'L', '"'};
  int i = 0;
  uint8_t commaCounter = 0;
  String number = "";
  uint8_t msgLength = 0;
  
  bool done = false;
  bool error = false;

  _recNumber = "";
  _buffer = "";
  _SIM.print("AT+CMGL=\"ALL\"\n\r");
  
  uint32_t lastNewData = millis(); 
  while (!done && !error)
  {
    if (_SIM.available())
    {
      lastNewData = millis();
      char value = _SIM.read();

      if (i < 13 && value != initMsg[i]) // Check if correct AT+CMGL="ALL" message
      {
        DEBUG_PRINTLN("Wrong init msg");
        error = true;
        break;
      }
      else if (value == ',')
      {
        commaCounter++;
        i = max(commaCounter * 10, 15); // avoid going back to lower than 13
      }

      if (commaCounter == 2) // Extract number
      {
        if (i > 21)
        {
          if (value == '"')
          {
            number = _buffer;
            _buffer = "";
          }
          else
          {
            _buffer += value;
          }
        }
      }
      else if (commaCounter == 12 && i > 120) // extract message length
      {
        _buffer += value;
        if (i == 122)
        {
          msgLength = _buffer.toInt();
          _buffer = "";
          commaCounter++;
          i = 130;
        }
      }

      if (msgLength != 0)
      {
        if(msgLength < 50)
        {
          if (i > 130 && i < 133 + msgLength)
          {
            if((byte)value != 0B00001101 && (byte)value != 0B00001010)
            {
              _buffer += value;
            }
          }
          else if(i > 133 + msgLength)
          {
            done = true;
          }
        }
        else
        {
          error = true;
          DEBUG_PRINTLN("To long message");
        }  
      }

      i++;
      if (i > 300)
        error = true;
    }

    if(millis() - lastNewData > 500)
    {
      done = true;
    }
  }

  if (number != "")
  {
    DEBUG_PRINTLN("Number: " + number);
    _recNumber = number;
    number = "";
  }

  if (msgLength != 0)
  {
    DEBUG_PRINTLN("Length: " + String(msgLength));
    msgLength = 0;
  }

  if (_buffer != "")
  {
    DEBUG_PRINTLN("Buff: " + _buffer);
  }

  _flush();
  i = 0;
  commaCounter = 0;
  number = "";
  msgLength = 0;

  return !error;
}

bool Sim800l::extractSettings()
{
  DEBUG_PRINTLN(_buffer);
  DEBUG_PRINTLN(_buffer.indexOf("#Set"));
  DEBUG_PRINTLN(_buffer.indexOf("#Status"));
  if(_buffer == "")
  {
    DEBUG_PRINTLN("Empty buffer, data lost");
    return false;
  }
  else
  {
    if(_buffer.indexOf("#Set") != -1)
    {
      DEBUG_PRINTLN("Setting Message");
      return _parseSettings();
    }
    else if(_buffer.indexOf("#Reset") != -1)
    {
      DEBUG_PRINTLN("Reset Message");
      userName = "Default";
      userNumber = "+33000000000";
      _newUser = true;
      return true;
    }
    else if(_buffer.indexOf("#Status") != -1)
    {
      DEBUG_PRINTLN("Status Message");
      String message = "Hi " + userName + ".\r\n";
      if(_isPowerPlugged)
      {
        message += "The power is normal.";
      }
      else
      {
        message += "The power is down !";
      }
      _sendSms(message);
      return true;
    }
    else
    {
      return false;
    }
  }
  
}

void Sim800l::setPowerSource(bool isPlugged)
{
  _isPowerPlugged = isPlugged;
}

void Sim800l::_flush()
{
  while (_SIM.available())
  {
    byte junk = _SIM.read();
  }
}

String Sim800l::_readSerialString()
{
  String output = "";

  int c;
  uint32_t startMillis = millis();
  do {
    c = _SIM.read();
    if (c >= 0)
    {
      output += (char)c;
    }
    else
    {
      return output;
    }
  } while(millis() - startMillis < 4000);
  
  DEBUG_PRINTLN("Time Out");
  return "";     // -1 indicates timeout
}

bool Sim800l::_msgRecieved()
{
  delay(3000);
  _buffer = _readSerialString();

  DEBUG_PRINTLN(_buffer);
  if (_buffer.indexOf("OK") != -1)
  {
    _buffer = "";
    return true;
  }
  else
  {
    _buffer = "";
    return false;
  }
}

bool Sim800l::_parseSettings()
{
  bool fullSettings = true;
  int idx = _buffer.indexOf("#Name:");
  if(idx != -1)
  {
    int i = 0;
    userName = "";
    while(_buffer[idx + 6 + i] != '#' && i < _buffer.length())
    {
      userName+=_buffer[idx + 6 + i];
      i++;
    }
    DEBUG_PRINTLN("Name: " + userName);
  }
  else
  {
    fullSettings = false;
  }
  

  idx = _buffer.indexOf("#Nbr:");
  if(idx != -1)
  {
    int i = 0;
    userNumber = "";
    if(_buffer.indexOf("this", idx + 5) != -1)
    {
      userNumber = _recNumber;
    }
    else
    {
      i = 0;
      while(_buffer[idx + 5 + i] != '#' && i < _buffer.length())
      {
        userNumber+=_buffer[idx + 5 + i];
        i++;
      }
    }
    DEBUG_PRINTLN("Number: " + userNumber);
  }
  else
  {
    fullSettings = false;
  }

  idx = _buffer.indexOf("#Interval:");
  if(idx != -1)
  {
    int i = 0;
    String interval = "";
    while(_buffer[idx + 10 + i] != '#' && i < _buffer.length())
    {
      interval+=_buffer[idx + 10 + i];
      i++;
    }

    uint16_t timeInterval = interval.toInt();
    setTimeInterval(timeInterval);
    DEBUG_PRINTLN("Interval: " + interval + " " + String(_timeInterval));
  }

  _newUser = fullSettings;

  if(_newUser)
  {
    _flush();
    _sendSms("Welcome " + String(userName) + " !");
  }

  _buffer="";

  //TO DO : return true if settings set in separate texts
  return fullSettings;
}
