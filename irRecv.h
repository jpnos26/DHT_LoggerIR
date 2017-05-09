#include <Arduino.h>
#include <IRremoteESP8266.h>
//////////////////////////////
// define pin for IR rx
//////////////////////////////
int RECV_PIN = 4; //an IR detector/demodulator is connected to GPIO pin 4
IRrecv irrecv(RECV_PIN);
void  dumpRaw (decode_results *results);
void  dumpCode (decode_results *results);
void  dumpInfo (decode_results *results);
