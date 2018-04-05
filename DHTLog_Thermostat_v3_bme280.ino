/*---------------------------------------------------
HTTP 1.1 Temperature & Humidity Webserver for ESP8266 
and ext value for Raspberry Thermostat

by Jpnos 2017  - free for everyone

 original developer :
by Stefan Thesen 05/2015 - free for anyone

Connect DHT21 / AMS2301 at GPIO2
versione 3.0.1
---------------------------------------------------*/
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include "time_ntp.h"
#include "DHT.h"
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include "RecIR.h"
#include "SendIR.h"
#include "ssd1306.h"
#include <SPIFFSEditor.h>
#include "bme280.h"
////////////// START CONFIGURAZIONE ///////////


// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

///Wifi Configuration
const char* wifissid = "ssid";
const char* wifipassword = "ssid_pwd";
const char* wifihostName = "DhtLogv3";
const char* httpusername = "admin";
const char* httppassword = "admin";
unsigned long ulNextWifiCheck;      // siamo in ap check wifi

int dhcp = 0; ////0 dhcp disable, 1 dhcp enabled
IPAddress ip(192,168,1,123);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
IPAddress dns(8,8,8,8);

// needed to avoid link error on ram check
extern "C" 
{
#include "user_interface.h"
}

//enable screen !!!Attenzione se non si ha lo schermo mettere a zero Qui tutte le abilitazioni
byte screen_on = 1;     //abilito la visualizzazione
byte irread_on = 1;     //abilito ricezione ir
byte irsend_on = 1;     //abilito trasmissione ir
byte dht22_on  = 0;     // abilito lettura  dht22
byte time_on   = 1;     //abilito accesso all'orario se usato solo come periferica di Thermostat e non si ha accesso a internet mettere a zero.
byte bme280_on = 1;     // abilito bm280 attenzione disabilitare dht22


int RELE_ON = 0;       // selezionare il tipo di on se optoisolato = 0 se diretto = 1;
int RELE_OFF= 1;       // selezionare il contrario di RELE_ON

int dht22_pin = 0  ;      // Selezionare il pin dov e collegato il dht 22
byte dht22_media = 0  ;   // 0 lettura dht pin diretta -- 1 lettura Mediata 

byte time_ok = 0;

// storage for Measurements; keep some mem free; allocate remainder
#define KEEP_MEM_FREE 18432
#define MEAS_SPAN_H 12
unsigned long ulMeasCount=0;          // values already measured
unsigned long ulNoMeasValues=0;       // size of array
unsigned long ulMeasDelta_ms;         // distance to next meas time
unsigned long ulNextMeas_ms;          // next meas time
unsigned long *pulTime;               // array for time points of measurements
float *pfTemp,*pfHum,*pfPres;         // array for temperature and humidity measurements
unsigned long ulReqcount;             // how often has a valid page been requested
unsigned long ulReconncount;          // how often did we connect to WiFi
int irRead;                           // enable read ir data
int chekEnable = 0;                   // comando ricevuto
int RELEPIN = 14;                     // pin per RELE
float tempHist=0.2;                   // temp histeresis
float dhtTemp,dhtTemp0,dhtTemp1;      // dht read temperature
float dhtUm,dhtUm0,dhtUm1;	          // dht read umidita
float dhtPres;                        // dht pressione
float setTemp;                        // set Temp to check
unsigned long ulNextntp;              // next ntp check
unsigned long timezoneRead();         // read time form internet db
String stato= "All OFF";              //stato sistema
int ihour_prev;                       // ora precedente

decode_results results; // Somewhere to store the results

//////////////////////////////
// DHT21 / AMS2301 pin
//////////////////////////////
#define DHTPIN dht22_pin

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// init DHT; 3rd parameter = 16 works for ESP8266@80MHz
DHT dht(DHTPIN, DHTTYPE,16);  

///// Initialize the async web
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      Serial.printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}
/////////////////////
// the setup routine
/////////////////////
void setup() 
{
    
    ////Init Serial
    Serial.begin(115200);
  	Serial.setDebugOutput(true);
  	Serial.println("Esp8266 WI-FI Rele Jpnos-2017 v1.0");
   
   ///inizializzo spiffs
   Serial.print ("Inizializzo SPIFFS .........\n");
   SPIFFS.begin();
   delay(100);
  
  // setup globals
  ulReqcount=0; 
  ulReconncount=0;
  irRead=0;
  //setup IR
  irrecv.setUnknownThreshold(MIN_UNKNOWN_SIZE);
  irrecv.enableIRIn();  // Start the receiver  
  irsend.begin();  //start the sender
  delay(10);
  
  //Setup Pin OUT
  pinMode(RELEPIN,OUTPUT);
  digitalWrite(RELEPIN,RELE_OFF);
  delay(10);

 //SETUP Screen
  if( screen_on ==1){
    initDisplay();
  }
// inizializzo bme280
  if (bme280_on==1){
    init_bme280();
  }
  
   ////Inizializzo Wifi
    
    Serial.print ("Inizializzo Wifi .........\n");
    WiFiStart(); ////Start Wifi
///Inizializzo ntp
     if (time_on == 1){
      ntpacquire();
     }
     
  //////// Inizializzo tabella per dati 
  uint32_t free=system_get_free_heap_size() - KEEP_MEM_FREE;
  ulNoMeasValues = 120;//free / (sizeof(float)*2+sizeof(unsigned long));  // humidity & temp --> 2 + time
  pulTime = new unsigned long[ulNoMeasValues];
  pfTemp = new float[ulNoMeasValues];
  pfHum = new float[ulNoMeasValues];
  pfPres = new float[ulNoMeasValues];
  if (pulTime==NULL || pfTemp==NULL || pfHum==NULL)
    {
      ulNoMeasValues=0;
      Serial.println("Errore in allocazione di memoria!");
    }
  else
    {
      Serial.print("Memoria Preparata per ");
      Serial.print(ulNoMeasValues);
      Serial.println(" data points.");
      float fMeasDelta_sec = 60;///MEAS_SPAN_H*3600./ulNoMeasValues;
      ulMeasDelta_ms = ( (unsigned long)(fMeasDelta_sec) ) * 1000;  // round to full sec
      Serial.print("La misura avviene ogni");
      Serial.print(ulMeasDelta_ms);
      Serial.println(" ms.");
      ulNextMeas_ms = millis()+ulMeasDelta_ms;
    }
  
  //Send OTA events to the browser
    setup_Server();
  
    //////Set ihour_prev
  ihour_prev = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
}

///////////////////
// (re-)start WiFi
///////////////////
///////////////////
// (re-)start WiFi
///////////////////
void WiFiStart()
    { 
      WiFi.disconnect();
      Serial.print("Connessione con :"); Serial.print(wifissid);Serial.print(" - ");Serial.print(wifipassword);Serial.print("\n");
      WiFi.mode(WIFI_STA);
      delay(100);
      int ret;
      bool autoConnect;
      //autoConnect = WiFi.getAutoConnect();
      // Connect to WiFi network
      if (dhcp == 0){
        WiFi.config(ip,gateway,subnet,dns);
        delay(100);
        }
      int timeout = 10;
      WiFi.begin(wifissid, wifipassword);
       while ( ((ret = WiFi.status()) != WL_CONNECTED) && timeout ) {
        delay(500);
        Serial.print(".");
        --timeout;
       }
      if ((ret = WiFi.status()) == WL_CONNECTED) {
        Serial.println("");
        Serial.print("WiFi connessa");
        // Print the IP address
        Serial.println(WiFi.localIP());
      }
      ulNextWifiCheck = millis()+500000;
}
 void ntpacquire() 
  {
  ///////////////////////////////
  // connect to NTP and get time
  ///////////////////////////////
  if ( screen_on == 1){
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,20);
    display.print("Load Time");
    display.display();
  }
  /*delay(50);
  ulSecs2000_timer=getNTPTimestamp();
  Serial.print("Ora Corrente da server NTP : " );
  Serial.println(epoch_to_string(ulSecs2000_timer).c_str());
  ulSecs2000_timer -= millis()/1000;  // keep distance to millis() counter
  ulNextntp=millis()+3600000;
  delay(1000);*/
  /////////////////////
  //timezoneRead()
  /////////////////////
  unsigned long timeInternet=timezoneRead();
  if (!timeInternet == 0)
    {
      ulSecs2000_timer=timeInternet;
      Serial.print("Ora Corrente da server Internet: " );
      Serial.println(epoch_to_string(ulSecs2000_timer).c_str());
      ulSecs2000_timer -= millis()/1000;  // keep distance to millis() counter
      ulNextntp=millis()+3600000;
      time_ok = 1;
    }
    else{
      time_ok=0;
    }
  if (ihour_prev ==0){
    ihour_prev = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
  }
  if ( screen_on==1){
    display.clearDisplay();
  }
 }

void DecodingIr()
{
decode_results  results;        // Somewhere to store the results
// Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    // Display a crude timestamp.
    uint32_t now = millis();
    Serial.printf("Timestamp : %06u.%03u\n", now / 1000, now % 1000);
    if (results.overflow){
      Serial.printf("WARNING: IR code is toint gethour();o big for buffer (>= %d). "
                    "This result shouldn't be trusted until this is resolved. "
                    "Edit & increase CAPTURE_BUFFER_SIZE.\n",
                    CAPTURE_BUFFER_SIZE);
    }
    // Display the library version the message was captured with.
    Serial.print("Library   : v");
    Serial.println(_IRREMOTEESP8266_VERSION_);
    Serial.println();

    // Output RAW timing info of the result.
    Serial.println(resultToTimingInfo(&results));
    yield();  // Feed the WDT (again)

    // Output the results as source code
    Serial.println(resultToSourceCode(&results));
    Serial.println("");  // Blank line between entries
    yield();  // Feed the WDT (again)
  }
}
void write_memory()
  {
  //Serial.print hour;
}
void ReadDht(){
    if (dht22_on == 1){
      dhtTemp0 = dht.readTemperature();
      dhtUm0 = dht.readHumidity();
      dhtPres = 0;
    }
    if (bme280_on == 1){
      dhtTemp0 = bme.readTemperature();
      dhtPres= bme.readPressure() / 100.0F;
      dhtUm0 = bme.readHumidity();
      
      Serial.print(dhtTemp0);Serial.print(" - ");Serial.print(dhtUm0);Serial.print(" - ");Serial.println(dhtPres);
    }
    if(dht22_on==1 || bme280_on==1){
      if (isnan(dhtTemp0) || isnan(dhtUm0)) 
      {
          Serial.println("Failed to read from DHT sensor!"); 
      }   
      else
      {
        if (dht22_media == 1)
        {
          if (ulMeasCount > 0){
            dhtTemp1 = (dhtTemp+dhtTemp0)/2;
            dhtTemp= dhtTemp1;
            dhtUm1= (dhtUm+dhtUm0)/2;
            dhtUm=dhtUm1;  
          }
          else 
          {
            dhtTemp=dhtTemp0;
            dhtUm=dhtUm0; 
          }
        }
        else{
          dhtTemp=dhtTemp0;
          dhtUm=dhtUm0;
        }
        if (time_ok == 1){
          pfHum[ulMeasCount%ulNoMeasValues] = dhtUm;
          pfTemp[ulMeasCount%ulNoMeasValues] = dhtTemp;
          pfPres[ulMeasCount%ulNoMeasValues] = dhtPres;
          pulTime[ulMeasCount%ulNoMeasValues] = millis()/1000+ulSecs2000_timer;
          Serial.print("Logging lettura: ");Serial.print(ulMeasCount%ulNoMeasValues);Serial.print(" - Temperature: ");
          Serial.print(pfTemp[ulMeasCount%ulNoMeasValues]);
          Serial.print(" deg Celsius - Humidity: "); 
          Serial.print(pfHum[ulMeasCount%ulNoMeasValues]);
          Serial.print("% - Pressure: ");
          Serial.print(pfPres[ulMeasCount%ulNoMeasValues]);
          Serial.print("% - Time: ");
          Serial.println(pulTime[ulMeasCount%ulNoMeasValues]);
          ulMeasCount++;
          int ihour = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
          if(ihour != ihour_prev) {
            save_logger();
            ihour_prev = ihour;
            ulMeasCount=0;
          }
        }
      }
    }
}
void save_logger() 
  {
  int ihour = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
  if (ihour == 0){
    ihour = 24;
  }
  String S_filename = "/datalog/"+ String(ihour-1) + ".csv";
  

  Serial.print("ihour. ");Serial.println(ihour);
  Serial.print("ihour_prev. ");Serial.println(ihour_prev);
  
    if(SPIFFS.exists(S_filename)==1)  {
      Serial.print("    Delete older file: ");Serial.println(S_filename);
      SPIFFS.remove(S_filename);
    }
  


  File datalogger = SPIFFS.open(S_filename, "a");
  if (!datalogger) {
    Serial.print(S_filename);Serial.println(" open failed");
  }
  for ( int i=0;i<=ulMeasCount-1;i++){
    datalogger.print(epoch_to_string_web(pulTime[i]));datalogger.print(",");datalogger.print(pfTemp[i],1);datalogger.print(",");datalogger.print(pfHum[i],1);datalogger.print(",");datalogger.println(pfPres[i],1);
    Serial.print("Scrivo file :");Serial.print(i);Serial.print(",");Serial.print(epoch_to_string_web(pulTime[i]));Serial.print(",");Serial.print(pfTemp[i],1);Serial.print(",");Serial.print(pfHum[i],1);Serial.print(",");Serial.println(pfPres[i],1);
  
  }
  Serial.print(S_filename);Serial.print(" size: ");Serial.println(datalogger.size());
  datalogger.close();
}

void DisplayText() {
  if ( screen_on == 1){
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0,5);
    display.print(epoch_to_date(millis()/1000+ulSecs2000_timer));
    display.setTextSize(2);
    display.setCursor(65,0);
    display.print(epoch_to_time(millis()/1000+ulSecs2000_timer));
    display.setCursor(0,27);
    display.print(pfTemp[(ulMeasCount-1)%ulNoMeasValues],1);
    display.print((char)247);
    display.setCursor(66,27);
    display.print(pfHum[(ulMeasCount-1)%ulNoMeasValues],1);
    display.print("%");
    display.setTextSize(1);
    display.setCursor(0,53);
    display.print(stato);
    display.display();
  }
}


/////////////
// main loop
/////////////
void loop() {

  // DO NOT REMOVE. Attend OTA update from Arduino IDE
   ArduinoOTA.handle();
   
///////////Aggiorno display
if ( screen_on == 1){
  DisplayText();
  delay(5);// The repeating section of the code
//
   //////////////////////////////
  // check if WLAN is connected
  //////////////////////////////
  if (millis() >= ulNextWifiCheck){
  	if (WiFi.status() != WL_CONNECTED){
  	WiFiStart();
   }
  }

  
}

///////////////////
  // do data logging
  ///////////////////
  if  (ulMeasCount==0){
        ReadDht();
      ulNextMeas_ms = millis()+ulMeasDelta_ms;
   }

  if (millis()>=ulNextMeas_ms) 
  {
      ReadDht();
   ulNextMeas_ms = millis()+ulMeasDelta_ms;
   //Serial.print(String(setTemp)+" - "+String(tempHist)+" - "+stato);
   switch (chekEnable)
      {
        case 0:
          stato="All OFF ";
          break;
        case 3 :
          if (setTemp >= pfTemp[(ulMeasCount-1)%ulNoMeasValues]+tempHist)
           {
            Serial.println("acceso rele :\n");
            digitalWrite ( RELEPIN,RELE_ON);
            stato="zona ON Set: "+String(setTemp,1);
            chekEnable = 3;
          }
          else if ( setTemp <= pfTemp[(ulMeasCount-1)%ulNoMeasValues])
            {
            Serial.println("spento rele: \n");
            digitalWrite ( RELEPIN , RELE_OFF) ;
            stato="zona OFF Set: "+String(setTemp,1);
            chekEnable = 100;
            }
          break;
         case 100 :
          if (setTemp >= pfTemp[(ulMeasCount-1)%ulNoMeasValues]+tempHist)
           {
            Serial.println("acceso rele :\n");
            digitalWrite ( RELEPIN,RELE_ON);
            stato="zona ON Set: "+String(setTemp,1);
            chekEnable = 3;
          }
          else if ( setTemp <= pfTemp[(ulMeasCount-1)%ulNoMeasValues])
            {
            Serial.println("spento rele: \n");
            digitalWrite ( RELEPIN , RELE_OFF) ;
            stato="zona OFF Set: "+String(setTemp,1);
            chekEnable = 100;
            }
            break;
          case 1 :
            stato="Cool ON Set: "+String(setTemp,1);
            break;
          case 2 :
            stato="Cool OFF ";
            break;
          case 99 :
          break;
          case 200:
            stato= "Rele Off ";
            break;
          case 201:
            stato = "Rele ON ";
            break;
      }
  }
  if (irRead == 1){
    if (irread_on ==1 ){
      DecodingIr();
    }
  }
	
  
  
 
  ///////////////////////////
  ///check NTP
  //////////////////////////
   if(time_on==1){
    if (millis()>=ulNextntp){
      ntpacquire();
    }
  }  
  
 
}


