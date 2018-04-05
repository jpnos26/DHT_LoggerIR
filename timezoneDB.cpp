 #include <ESP8266WiFi.h>
 #include <ArduinoJson.h>

 
 String keyDB = "key_db";        ///chiave database internet
 const char* host = "api.timezonedb.com";   /// host timezoneDB
 unsigned long timezoneRead()
 {
    Serial.print("Ora Corrente da server Internet: " );
    WiFiClient client;
      const int httpPort = 80;
      if (!client.connect(host, httpPort)) {
        Serial.println("connection failed");
        return(0);
      }
      // We now create a URI for the request
      String url = "/v2/get-time-zone?key="+keyDB+"&format=json&by=zone&zone=Europe/Rome";
      Serial.print("Requesting URL: ");
      Serial.println(url);

      // This will send the request to the server
      client.print("GET " + url + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" +
                   "Content-Type: application/json\r\n" + 
                   "Connection: close\r\n\r\n");
      delay(10);
      // Read all the lines of the reply from server and print them to Serial
      String json = "";
      unsigned long startTime = millis();
      unsigned long httpResponseTimeOut = 10000; // 10 sec
      while (client.connected() && ((millis() - startTime) < httpResponseTimeOut)) {
          if (client.available()) {
            String line = client.readStringUntil('\r');
            if (line.charAt(1) == '{') {
              line.trim();
              int jsonfine = line.indexOf('}');
              Serial.print(jsonfine);
              line.substring(0,jsonfine-1);
              json += line;
              }
          else {
                    Serial.print(".");
                    delay(100);
                }
        }
      }
      delay(20);
      const size_t bufferSize = JSON_OBJECT_SIZE(13) + 250;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      
      Serial.println("Got data:");
      Serial.println(json);
      JsonObject& root = jsonBuffer.parseObject(json);
      if (!root.success()){
        Serial.println("parseObject() failed");
        return (0);
        }
      String data = root["status"];
      int data1= root["gmtOffset"]; 
      unsigned long data2 = root["timestamp"]; 
      if (data =="OK"){
          data2 -= 946684800UL;
          return(data2);
      }
      
      Serial.println("Data ");
      Serial.print(data);
      Serial.print(data1);
      Serial.print(data2);

      Serial.println("closing connection");  
    
 }


