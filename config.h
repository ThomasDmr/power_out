#pragma once

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINTLN(x)
#endif

#define bitSet(x, n)        ((x) |= bit(n))

const int LED_R         = 11;
const int LED_G         = 10;
const int PWR_FEEDBACK  = A0;
const int PWR_SELECT    = 5;
const int TX_PIN        = 3;
const int RX_PIN        = 2;
const int RESET_PIN     = 4; 

const int MUX_STATUS    = 6;    // HIGH = BATTERY, LOW = USB 
const int CHARGE_STATUS = 7;    // HIGH when loaded
