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
int id;
//bool watchdog; //watchdog alternates between true and false every second or so to display the active connection (it stops when there is no Data flowing)
int clients, temp, volt, cpuLoad;

float rx, tx, rxSpeed, currentRx, lastRx, txSpeed, currentTx, lastTx, ram;

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

        //        watchdog = !watchdog; //alternate the watchdog value when data is received

        // Extract Data from Json to string array
        //        int id = doc["id"];
        id = doc["id"];
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
        printOutData();
      } else {
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
  Serial.begin(500000);

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
  delay(1000);

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
  //  delay(50); //adjust the delay for lower latency (100ms works good, 500ms is fine, 1000ms is a bit laggy)


}

void dataPrep() {
  //int clients, temp, volt, rx, tx, cpuLoad, ram;
  //float rx, tx, rxSpeed, txSpeed, ram;

  clients = values[1].toInt();
  temp = values[2].toInt();
  volt = values[3].toInt();
  
  if (id == 4) {
    rx = floatProcessing(values[4]);
    currentRx = rx;
    rxSpeed = currentRx - lastRx;
    lastRx = currentRx;
  }
  if (id == 5){
    tx = floatProcessing(values[5]);
    currentTx = tx;
    txSpeed = currentTx - lastTx;
    lastTx = currentTx;
  }
  
  cpuLoad = values[6].toInt();
  ram = values[7].toFloat() / 1048576;

}

float floatProcessing(String data) {
  //RX PROCESSING
  //================================================================
  String toFloatStr;
  int str_len = values[4].length() + 1;
  //  Serial.println();
  //  Serial.print("length = "); Serial.println(str_len);
  char rxChar[str_len];
  data.toCharArray(rxChar, str_len);
  //  Serial.println();
  for (int i = 0; i < str_len; i++) {
    if (isDigit(rxChar[i])) {
      toFloatStr += rxChar[i];
      //      Serial.print("Received data = ");
      //      Serial.println(rxChar[i]);
    }
  }
  return toFloatStr.toFloat();

  toFloatStr = "";

  //END OF RX PROCESSING
  //================================================================
}

void printOutData() {
  dataPrep();
  Serial.println();
  Serial.println("===============================================");
  Serial.print("Millis      : "); Serial.println(millis() / 1000);
  Serial.print("DHCP Client : "); Serial.print(clients);  Serial.println(" Clients");
  Serial.print("Temp        : "); Serial.print(temp);     Serial.println(" C");
  Serial.print("Volt        : "); Serial.print(volt);     Serial.println(" Volts");
  Serial.print("RX Bytes    : "); Serial.print(rx / 1000000000.00);       Serial.println(" GB");
  Serial.print("RX Speed    : "); Serial.print(rxSpeed / 1000000.00);       Serial.println(" MB/s");
  Serial.print("TX Bytes    : "); Serial.print(tx / 1000000000.00);       Serial.println(" GB");
  Serial.print("TX Speed    : "); Serial.print(txSpeed / 1000000.00);     Serial.println(" MB/s");
  Serial.print("CPU Load    : "); Serial.print(cpuLoad);  Serial.println(" %");
  Serial.print("Free RAM    : "); Serial.print(ram);      Serial.println(" MB");
  Serial.print("Uptime      : "); Serial.println(values[8]);
  //  Serial.println("Clients : " + values[1] + " Clients");    //Display the Value No.1
  //  Serial.println("Temp    : " + values[2] + " C");//Display the Value No.2
  //  Serial.println("Volt    : " + values[3] + " V");//Display the Value No.3
  //  Serial.println("WAN RX d: " + values[4] + " GB");//Display the Value No.4
  //  Serial.println("WAN TX d: " + values[5] + " GB");//Display the Value No.4
  //  Serial.println("CPU Load: " + values[6] + " %");//Display the Value No.4
  //  Serial.println("Free RAM: " + values[7] + " MB");//Display the Value No.4
  //  Serial.println("Uptime  : " + values[8]);
  //  Serial.print  ("WAN RX  : "); Serial.print(rx); Serial.println(" GB");
  //  Serial.print  ("WAN TX  : "); Serial.print(tx); Serial.println(" GB");

  Serial.println();
}
