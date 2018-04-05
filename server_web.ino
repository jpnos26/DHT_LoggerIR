

void setup_Server(){

  Serial.println ("Inizializzo OTA .........\n");
  String hostNAME = wifihostName;
  hostNAME += "-";
  hostNAME += String(ESP.getChipId(), HEX);
  Serial.print("set OTA+WiFi hostname: "); Serial.println(hostNAME);Serial.print("\n");
  WiFi.hostname(hostNAME);
  ArduinoOTA.onStart([]() { events.send("Update Start", "ota"); });
  ArduinoOTA.onEnd([]() { events.send("Update End", "ota"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress/(total/100)));
    events.send(p, "ota");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if(error == OTA_AUTH_ERROR) events.send("Auth Failed", "ota");
    else if(error == OTA_BEGIN_ERROR) events.send("Begin Failed", "ota");
    else if(error == OTA_CONNECT_ERROR) events.send("Connect Failed", "ota");
    else if(error == OTA_RECEIVE_ERROR) events.send("Recieve Failed", "ota");
    else if(error == OTA_END_ERROR) events.send("End Failed", "ota");
  });
  
  ArduinoOTA.setHostname((const char *)hostNAME.c_str());
  
  ArduinoOTA.begin();

  MDNS.addService("http","tcp",80);
  
  
  //////Setto web Server
  Serial.print ("Inizializzo Web Server .........\n");
  
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  events.onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);

  server.addHandler(new SPIFFSEditor(httpusername,httppassword));
  
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  
   server.on("/Restart", HTTP_GET, [](AsyncWebServerRequest *request){
      delay(100);
      Serial.println("Restart ---");
      ESP.restart();
   });
   server.on("/logger", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", String(ESP.getFreeHeap()));
      Serial.println("Save Logger ---");
      int ihour = epoch_to_hour(millis()/1000+ulSecs2000_timer).toInt();
      String S_filename = "/datalog/"+ String(ihour) + ".csv";
      if(SPIFFS.exists(S_filename)==1)  {
          Serial.print("    Delete older file: ");Serial.println(S_filename);
          SPIFFS.remove(S_filename);
        }
      File datalogger = SPIFFS.open(S_filename, "w");
       if (!datalogger) {
          Serial.print(S_filename);Serial.println(" open failed");
        }
        for ( int i=0;i<=ulMeasCount-1;i++){
          datalogger.print(epoch_to_string_web(pulTime[i]));datalogger.print(",");datalogger.print(pfTemp[i],1);datalogger.print(",");datalogger.print(pfHum[i],1);datalogger.print(",");datalogger.println(pfPres[i],1);
          Serial.print("Scrivo file :");Serial.print(i);Serial.print(",");Serial.print(epoch_to_string_web(pulTime[i]));Serial.print(",");Serial.print(pfTemp[i],1);Serial.print(",");Serial.print(pfHum[i],1);Serial.print(",");Serial.println(pfPres[i],1);
          }
      Serial.print(S_filename);Serial.print(" size: ");Serial.println(datalogger.size());
      datalogger.close();
   });
   
   server.on("/dati", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("\nGET /dati");
    int iIndex= (ulMeasCount-1)%ulNoMeasValues;
    String json = "{";
    json += "\"S_heap\":"+String(ESP.getFreeHeap());
    json += ", \"S_hostname\":\""+WiFi.hostname()+"\"";
    json += ", \"S_temperature\":"+String(pfTemp[iIndex]);
    json += ", \"S_setTemp\":"+String(setTemp);
    json += ", \"S_humidity\":"+String(pfHum[iIndex]);
    json += ", \"S_pressure\":"+String(pfPres[iIndex]);
    json += ", \"S_relestatus\":\"" +stato+"\"";
    json += ", \"S_control\":"+String(chekEnable);
    json += ", \"S_time\":\"" + String(epoch_to_string(pulTime[iIndex]))+"\"";
    json += ", \"S_dht22\":"+String(dht22_on);
    json += ", \"S_bme280\":"+String(bme280_on);
    json += "}";
    Serial.print("Json: \n");Serial.print(json);Serial.print("\n");
    request->send(200, "application/json", json);
  });

  server.on("/releON", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.println("Rele ON  ---");
      chekEnable = 201;
      setTemp = 0;
      digitalWrite ( RELEPIN , RELE_ON);
      stato="Rele ON  ";
      ulNextMeas_ms = millis();
      //request->send(200, "text/plain", String(ESP.getFreeHeap()));
      request->redirect("/index.html");
  });
  server.on("/releOFF", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.println("Rele OFF  ---");
      chekEnable = 200;
      setTemp = 0;
      digitalWrite ( RELEPIN , RELE_OFF);
      stato="Rele OFF  ";
      ulNextMeas_ms = millis();
      //request->send(200, "text/plain", String(ESP.getFreeHeap()));
      request->redirect("/index.html");
  });

   server.on("/zoneOFF", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.println("Zona OFF  ---");
      chekEnable = 100;
      setTemp = 0;
      ulNextMeas_ms = millis();
      //request->send(200, "text/plain", String(ESP.getFreeHeap()));
      request->redirect("/index.html");
  });
  
   server.on("/zoneON", HTTP_GET, [](AsyncWebServerRequest *request){
      int params = request->params();
      int i = 0;
      AsyncWebParameter* p = request->getParam(i);
      setTemp = p->name().toFloat();
      Serial.print("Zona ON  ---");Serial.println(setTemp);
      Serial.printf("HEADER[%s]: %s\n", p->name().c_str(), p->value().c_str());
      chekEnable = 3;
      ulNextMeas_ms = millis();
      //request->send(200, "text/plain", String(ESP.getFreeHeap()));
      request->redirect("/index.html");
  });
  
  server.on("/testLan", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("TestLan  ---");
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  
  server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request){
     Serial.println("Clear  ---");
     chekEnable = 0;
     setTemp = 0;
     digitalWrite ( RELEPIN , RELE_OFF);
     irsend.sendRaw(Off,iRlen,38);
     stato="All OFF ";
    //request->send(200, "text/plain", String(ESP.getFreeHeap()));
    request->redirect("/index.html");
  });

  server.on("/irDecoder", HTTP_GET, [](AsyncWebServerRequest *request){
      int params = request->params();
      int i = 0;
      AsyncWebParameter* p = request->getParam(i);
      int sParam = p->name().toInt();
      Serial.print("IrDecoder ON  ---");Serial.println(sParam);
      switch (sParam)
        {
        case 101:
          irRead = 0;
          stato = "All OFF";
          break;
        case 102:
          irRead = 1;
          stato = "Decoder ON";
          break;
        default: 
          irRead = 0;
          stato = "All OFF";
          break;  
      }
      //request->send(200, "text/plain", String(sParam));
      request->redirect("/index.html");
  });
  server.on("/irSender", HTTP_GET, [](AsyncWebServerRequest *request){
      int params = request->params();
      int i = 0;
      AsyncWebParameter* p = request->getParam(i);
      int sParam = p->name().toInt();
      Serial.print("IrSEnder invia  ---");Serial.println(sParam);
      switch (sParam)
      {
        case 20:
         irsend.sendRaw(On20,iRlen,38);
         chekEnable = 1;
         setTemp= 20.0;
         stato="Cool ON Set: "+String(setTemp,1);
         break;
        case 21:
         irsend.sendRaw(On21,iRlen,38);
         chekEnable = 1;
         setTemp= 21.0;
         stato="Cool ON Set: "+String(setTemp,1);
         break;
        case 22:
         irsend.sendRaw(On22,iRlen,38);
         chekEnable = 1;
         setTemp= 22.0;
         stato="Cool ON Set: "+String(setTemp,1);
         break;
        case 23:
         irsend.sendRaw(On23,iRlen,38);
         chekEnable = 1;
         setTemp= 23.0;
         stato="Cool ON Set: "+String(setTemp,1);
         break;
        case 24:
         irsend.sendRaw(On24,iRlen,38);
         chekEnable = 1;
         setTemp= 24.0;
         stato="Cool ON Set: "+String(setTemp,1);
         break;
        case 25:
         irsend.sendRaw(On24,iRlen,38);
         chekEnable = 1;
         setTemp= 25.0;
         stato="Cool ON Set: "+String(setTemp,1);
         break;
        case 26:
         irsend.sendRaw(On24,iRlen,38);
         chekEnable = 1;
         setTemp= 26.0;
         stato="Cool ON Set: "+String(setTemp,1);
         break;
        case 99:
         irsend.sendRaw(Off,iRlen,38);
         chekEnable = 2;
         stato="Cool OFF";
         break;
        default:
         break;
    }
    request->redirect("/index.html");
  });
  server
    .serveStatic("/", SPIFFS, "/")
    .setDefaultFile("index.html")
    .setCacheControl("max-age=300");

  
   server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if(request->contentLength()){
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for(i=0;i<headers;i++){
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
     Serial.print("Parametri");
    for(i=0;i<params;i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isFile()){
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if(p->isPost()){
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
     Serial.print("On File Upload");
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.print("On request Body ");
    if(!index)
     Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });
  
 
    
  server.begin();



  //Client
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  //#ifdef TTD
  //  Serial.println("SEND BOOTUP");
  //  HTTPClient clienthttp_SST;
  //  const char* host="http://www.google-analytics.com/collect";
  //  String eventData = "v=1&t=event&tid=UA-89261240-1&cid=555&ec=SST"+String(VERSION)+"&ea=BOOTUP&el="+String(ESP.getChipId(),HEX);
  //  clienthttp_SST.begin(host);
  //  clienthttp_SST.addHeader("User-agent", "Mozilla/5.0 (X11; Linux x86_64; rv:12.0) Gecko/20100101 Firefox/21.0");
  //  clienthttp_SST.POST(eventData);
  //  clienthttp_SST.writeToStream(&Serial);
  //  clienthttp_SST.end();
  //  Serial.println("BOOTUP CLOSED");
  //#endif
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  ///server.begin();
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
}  


