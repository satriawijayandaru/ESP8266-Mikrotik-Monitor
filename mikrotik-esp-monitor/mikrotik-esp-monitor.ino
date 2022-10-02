/*
github.com/neogoth/mikrotik-esp-monitor

Copyright (C) 2021 Neogoth

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>


// Enter WiFi Credentials here
const char* ssid = "XDA";
const char* password = "namaguee";

String values[50]; //Set max Array Size
bool watchdog; //watchdog alternates between true and false every second or so to display the active connection (it stops when there is no Data flowing)


ESP8266WebServer server(80); //you can define the Server Port here. Default is 80(HTTP)



void setVars() {
    String postBody = server.arg("plain");
    Serial.println(postBody);
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, postBody);
    
    if (error) {
        // if the file didn't open, print an error:
        Serial.print(F("Error parsing JSON "));
        Serial.println(error.c_str());
 
        String msg = error.c_str();
 
        server.send(400, F("text/html"),
                "Error in parsin json body! <br>" + msg);
 
    } else {
        JsonObject postObj = doc.as<JsonObject>();
 
        Serial.print(F("HTTP Method: "));
        Serial.println(server.method());
 
        if (server.method() == HTTP_POST) {
            if (postObj.containsKey("value")) {
 
                Serial.println(F("done."));
 
                // Here store data or doing operation
                
                watchdog = !watchdog; //alternate the watchdog value when data is received

                // Extract Data from Json to string array
                int id = doc["id"];         
                String value = doc["value"]; //Extract the Value from Json
                values[id] = value;         //Write the value to the defined place in the array

 
                // Create the response
                // To get the status of the result you can get the http status so
                DynamicJsonDocument doc(512);
                doc["status"] = "OK";
 
                Serial.print(F("Stream..."));
                String buf;
                serializeJson(doc, buf);


                server.send(201, F("application/json"), buf);
                Serial.print(F("done."));
 
            }else {
                DynamicJsonDocument doc(512);
                doc["status"] = "KO";
                doc["message"] = F("No data found, or incorrect!");
 
                Serial.print(F("Stream..."));
                String buf;
                serializeJson(doc, buf);
 
                server.send(400, F("application/json"), buf);
                Serial.print(F("done."));
            }
        }
    }
}
 
 
// Define routing
void restServerRouting() {
    server.on("/", HTTP_GET, []() {
        server.send(200, F("text/html"),
            F("Welcome to the REST Web Server"));
    });
    // handle post request
    server.on(F("/setVars"), HTTP_POST, setVars);
}
 
// Manage not found URL
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}





 
void setup(void) {
  Serial.begin(115200);

//-------------WIFI--------------------------
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
 
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(3000);

//-------------SERVER------------------------
  // Activate mDNS this is used to be able to connect to the server
  // with local DNS hostmane esp8266.local
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
  // Set server routing
  restServerRouting();
  // Set not found response
  server.onNotFound(handleNotFound);
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  
}
 
void loop(void) {
  server.handleClient();
  delay(100); //adjust the delay for lower latency (100ms works good, 500ms is fine, 1000ms is a bit laggy)

  
  
  //Write your own Stuff here or modify the Output
  Serial.println("Clients:" + values[1]);    //Display the Value No.1
  Serial.println("Temp: " + values[2] + "C");//Display the Value No.2
  Serial.println("Volt: " + values[3] + "V");//Display the Value No.3       
  Serial.println("WAN : " + values[4].toInt / 1000 + "MBps");//Display the Value No.4

  Serial.println();

}
