// Sim800l.h
/**
 * Sim800l is a custom library based on the Sim800l module that allows to send and receive SMS over the GSM band. 
 * This library is built around a project allowing to detect power drop outs and warning the owner in case it happens. 
 */

#ifndef Sim800l_h
#define Sim800l_h
#include <SoftwareSerial.h>
#include "Arduino.h"

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINTLN(x)
#endif

class Sim800l		
{									
  public:

	Sim800l(uint8_t pinRX, uint8_t pinTX, uint8_t pinRST, int baudRate = 4800);
 	void begin();	
 	void reset(); 
	void updateSerial();
	bool setToReceptionMode();
	bool checkIfNewSMS();
	bool delAllSms();
	bool hasCorrectSignal(int callInterval_second);
	bool parseSmsData();
	bool isUserConfigurated();
	bool newUserConf();
	void setTimeInterval(uint16_t timeInMinutes);
	
	void setLipoValue(int charge);
	void setPowerSource(bool isPlugged);

	bool sendWarning();
	bool sendPowerBack();

	bool extractSettings();
	

	String 	userNumber;
	String 	userName;	 
	bool 	firstCall;


  private:
	String 			_msgBuffer;
	SoftwareSerial 	_SIM; 
	int 			_baudRate;
	uint8_t 		_pinRST;
	String 			_buffer;
	String 			_recNumber;
	bool			_newUser;
	uint32_t		_timeInterval;
	int 			_charge;
	bool			_isPowerPlugged;
	bool 			_firstAccuWarning = true;
	
	String 	_readSerialString();
	bool 	_msgRecieved();
	bool 	_parseSettings();
	void 	_flush();
	bool 	_sendSms(String text);
  	
};

#endif 