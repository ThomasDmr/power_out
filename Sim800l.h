// Sim800l.h
/**
 * Sim800l is a custom library based on the Sim800l module that allows to send and receive SMS over the GSM band. 
 * This library is built around a project allowing to detect power drop outs and warning the owner in case it happens. 
 */

#pragma once

#include <SoftwareSerial.h>
#include "Arduino.h"

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINTLN(x)
#endif

namespace Sim800
{
enum msgType {ERROR, RESET, STATUS, SETTINGS, STOP};

class Sim800l		
{									
	public:

	Sim800l(uint8_t pinRX, uint8_t pinTX, uint8_t pinRST, int baudRate = 4800);
	void 	begin();	
	void 	sendStringToSIM(String msgToSend);
	void 	checkForIncommingData();
	int	 	hasCorrectSignal(int callInterval_second);
	bool 	setToReceptionMode();


	bool 	newSmsAvailable();
	bool 	parseSmsData();
	msgType extractTypeFromSms();
	bool 	delAllSms();
	
	bool 	sendSms(String smsText, String number, String name);

	String getUserName();
	String getUserNumber();
	
	private:

	void 	m_flush();
	bool 	m_messageWellReceived();
	void 	m_waitForResponse(int timeOut, bool keepMessage = true);
	void 	m_removeCharUntil(String character, int extraChar = 0);
	bool 	m_parseSettingsMsg();
	void 	m_clearLastMessage();

	SoftwareSerial 	m_SIM; 
	int 			m_baudRate, m_lastMsgSentLength;
	uint8_t 		m_pinRST;
	String 			m_inputBuffer, m_lastReceivedMsg; 
	String			m_userName, m_userNumber;
	uint32_t		m_inputTimer; 	
};
} // namespace Sim800