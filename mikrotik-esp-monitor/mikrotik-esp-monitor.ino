#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = "XDA";
const char* password = "namaguee";

String values[50]; //Maximum data from mikrotik
int id;
bool watchdog; //watchdog alternates between true and false every second or so to display the active connection (it stops when there is no Data flowing)
int clients, temp, volt, cpuLoad;

float rx, tx, rxSpeed, currentRx, lastRx, txSpeed, currentTx, lastTx, ram, ramPercent, ramUsage, ramUsagePercent;

unsigned long previousMillis = 0;
const long interval = 50;

ESP8266WebServer server(80);


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
  lcd.init();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); \
    lcd.print(".");
  }
  lcd.clear();
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  lcd.setCursor(0, 0);
  lcd.print("Connected to");
  lcd.print(ssid);
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
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
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

  }
}

void dataPrep() {
  //int clients, temp, volt, rx, tx, cpuLoad, ram;
  //float rx, tx, rxSpeed, txSpeed, ram;
  if (id == 1) {
    clients = values[1].toInt();
  }
  if (id == 2) {
    temp = values[2].toInt();
  }
  if (id == 3) {
    volt = values[3].toInt();
  }
  if (id == 4) {
    rx = floatProcessing(values[4]);
    currentRx = rx;
    rxSpeed = currentRx - lastRx;
    lastRx = currentRx;
  }
  if (id == 5) {
    tx = floatProcessing(values[5]);
    currentTx = tx;
    txSpeed = currentTx - lastTx;
    lastTx = currentTx;
  }
  if (id == 6) {
    cpuLoad = values[6].toInt();
  }
  if (id == 7) {
    ram = values[7].toFloat() / 1048576;
    ramUsage = 256.00 - ram;
    ramPercent = (ram / 256.00) * 100;
    ramUsagePercent = (ramUsage / 256.00 ) * 100;
  }
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
  Serial.print("DHCP Client : "); Serial.print(clients);                  Serial.println(" Clients");
  Serial.print("Temp        : "); Serial.print(temp);                     Serial.println(" C");
  Serial.print("Volt        : "); Serial.print(volt);                     Serial.println(" Volts");
  Serial.print("RX Bytes    : "); Serial.print(rx / 1000000000.00);       Serial.println(" GB");
  Serial.print("RX Speed    : "); Serial.print(rxSpeed / 1000000.00);     Serial.println(" MB/s");
  Serial.print("TX Bytes    : "); Serial.print(tx / 1000000000.00);       Serial.println(" GB");
  Serial.print("TX Speed    : "); Serial.print(txSpeed / 1000000.00);     Serial.println(" MB/s");
  Serial.print("CPU Load    : "); Serial.print(cpuLoad);                  Serial.println(" %");
  Serial.print("Free RAM    : "); Serial.print(ram);                      Serial.println(" MB");
  Serial.print("RAM Usage   : "); Serial.print(ramUsage);                 Serial.println(" MB");
  Serial.print("Free Ram %  : "); Serial.print(ramPercent);               Serial.println(" %");
  Serial.print("Ram usage % : "); Serial.print(ramUsagePercent);
  Serial.println(" %");
  Serial.print("Uptime      : "); Serial.println(values[8]);

  Serial.println("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
  
  int lcdRxLevel = map((rxSpeed / 1000000.00), 0, 50, 0, 15);
  int lcdTxLevel = map((txSpeed / 1000000.00), 0, 50, 0, 15);
  
  Serial.print("RX Percent ");
  Serial.print(lcdRxLevel);
  Serial.print(" = ");
  for (int i; i < lcdRxLevel; i++) {
    Serial.print("#");
  }
  Serial.println();
  
  Serial.print("TX Percent ");
  Serial.print(lcdTxLevel);
  Serial.print(" = ");
  for (int j; j < lcdTxLevel; j++) {
    Serial.print("#");
  }
  
  Serial.println();
  Serial.println();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("RX = ");
  lcd.print(rxSpeed / 1048576.00);
  lcd.print(" MB/s");

  lcd.setCursor(0, 1);
  lcd.print("TX = ");
  lcd.print(txSpeed / 1048576.00);
  lcd.print(" MB/s");
}
