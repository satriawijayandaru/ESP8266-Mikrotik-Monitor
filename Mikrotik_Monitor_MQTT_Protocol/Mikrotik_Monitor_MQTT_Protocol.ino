#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "ClickButton.h"
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>

WiFiServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);

int debugEn = 1;
char* ssid = "";
char* password = "";

const int EEPROM_SIZE = 512;
int wifi_ssid_address = 0;
int wifi_password_address = 32;

const char* ssidAp = "ESP8266 Hotspot";
const char* passwordAp = "password";

String selectedSSID;
String selectedPassword;
String storedSSID = "";
String storedPassword = "";

int btn = 0;
int btnFunc = 0;
ClickButton btn1(btn, LOW, CLICKBTN_PULLUP);

const int ledPin = LED_BUILTIN;
int ledState = LOW;
unsigned long previousMillis = 0;
long interval;
int eepromStat;

//const char* mqtt_server = "192.168.88.200";
const char* mqtt_server = "11.11.11.111";

WiFiClient espClient;
PubSubClient client(espClient);

String strMikrotikCPU, strMikrotikRX, strMikrotikTX, strMikrotikTemp;
String strMikrotikVoltage, strMikrotikRAM;
int cpuUsage, mikrotikTemp, mikrotikVoltage, ramAvailPercent;
double rxFloat, txFloat, previousRxFloat, previousTxFloat;
double speedRx, speedTx, ramAvailMB;
unsigned long currentTime, previousTime;

byte blankChar[] = {
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F
};

byte rxChar[] = {
  0x1C,
  0x14,
  0x18,
  0x14,
  0x00,
  0x14,
  0x08,
  0x14
};

byte txChar[] = {
  0x1C,
  0x08,
  0x08,
  0x08,
  0x00,
  0x14,
  0x08,
  0x14
};

void setup() {
  pinMode(btn, INPUT);
  pinMode(ledPin, OUTPUT);
  btn1.debounceTime   = 20;   // Debounce timer in ms
  btn1.multiclickTime = 250;
  btn1.longClickTime = 2000;
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, blankChar);
  lcd.createChar(1, rxChar);
  lcd.createChar(2, txChar);
  Serial.begin(115200);
  wifiCredentialCheck();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  stat();
  resetBtn();
  wifiSelectGUI();

}

void lcdPrint() {
  int rxBar = (speedRx / 60) * 16;
  int txBar = (speedTx / 60) * 16;
  if (rxBar > 16) {
    rxBar = 16;
  }
  if (txBar > 16) {
    txBar = 16;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(1);
  lcd.print(speedRx, 3);
  lcd.setCursor(8, 0);
  lcd.write(2);
  lcd.print(speedTx, 3);
  lcd.setCursor(15, 0);
  lcd.print("M");

  lcd.setCursor(0, 1);
  lcd.write(1);
  if (rxFloat > 100.0) {
    lcd.print(rxFloat, 1);
  }
  if (rxFloat < 100.0) {
    lcd.print(rxFloat, 2);
  }
  lcd.setCursor(8, 1);
  lcd.write(2);
  if (txFloat > 100.0) {
    lcd.print(txFloat, 1);
  }
  if (txFloat < 100.0) {
    lcd.print(txFloat, 2);
  }
  lcd.setCursor(15,1);
  lcd.print("G");
}

void callback(char* topic, byte* payload, unsigned int length) {
  //    Serial.print("Incoming [");
  //    Serial.print(topic);
  //    Serial.print("] -> ");
  //    for (int i = 0; i < length; i++) {
  //      Serial.print((char)payload[i]);
  //    }
  //    Serial.println();

  if (strcmp(topic, "mikrotikTemp") == 0) {

    for (int i = 0; i < length; i++) {
      strMikrotikTemp += (char)payload[i];
    }
    mikrotikTemp = strMikrotikTemp.toInt();
    if (debugEn == 1) {
      Serial.print("Temp     = ");
      Serial.print(mikrotikTemp);
      Serial.println(" C");
    }
    strMikrotikTemp = "";
  }

  if (strcmp(topic, "mikrotikVoltage") == 0) {
    for (int i = 0; i < length; i++) {
      strMikrotikVoltage += (char)payload[i];
    }
    mikrotikVoltage = strMikrotikVoltage.toInt();
    if (debugEn == 1) {
      Serial.print("Voltage  = ");
      Serial.print(mikrotikVoltage);
      Serial.println(" V");
    }
    strMikrotikVoltage = "";
  }

  if (strcmp(topic, "mikrotikRX") == 0) {
    currentTime = millis();
    int time = round(currentTime - previousTime);

    for (int i = 0; i < length; i++) {
      if (isDigit((char)payload[i])) {
        strMikrotikRX += (char)payload[i];
      }
    }
    previousTime = currentTime;
    rxFloat = strMikrotikRX.toFloat() / (1074000000.0 );
    speedRx = (strMikrotikRX.toFloat() - previousRxFloat) / 107400.0;
    previousRxFloat = strMikrotikRX.toFloat();


    if (debugEn == 1) {
      Serial.print("RX Bytes = ");
      Serial.print(rxFloat, 3);
      Serial.println(" GB");
      Serial.print("RX Speed = ");
      Serial.print(speedRx, 2);
      Serial.println(" Mbps");
    }
    strMikrotikRX = "";
  }

  if (strcmp(topic, "mikrotikTX") == 0) {
    for (int i = 0; i < length; i++) {
      if (isDigit((char)payload[i])) {
        strMikrotikTX += (char)payload[i];
      }
    }
    txFloat = strMikrotikTX.toFloat() / (1074000000.0 );
    speedTx = (strMikrotikTX.toFloat() - previousTxFloat) / 107400.0;
    previousTxFloat = strMikrotikTX.toFloat();
    if (debugEn == 1) {
      Serial.print("TX Bytes = ");
      Serial.print(txFloat, 3);
      Serial.println(" GB");
      Serial.print("TX Speed = ");
      Serial.print(speedTx, 2);
      Serial.println(" Mbps");
    }
    strMikrotikTX = "";
  }

  if (strcmp(topic, "mikrotikCPU") == 0) {
    for (int i = 0; i < length; i++) {
      strMikrotikCPU += (char)payload[i];
    }
    cpuUsage = strMikrotikCPU.toInt();
    if (debugEn == 1) {
      Serial.print("CPU Usage= ");
      Serial.print(cpuUsage );
      Serial.println(" %");
    }
    strMikrotikCPU = "";
  }

  if (strcmp(topic, "mikrotikRAM") == 0) {
    for (int i = 0; i < length; i++) {
      strMikrotikRAM += (char)payload[i];
    }
    ramAvailMB = strMikrotikRAM.toInt() / 1049000;
    ramAvailPercent = ((256 - ramAvailMB) / 256.0) * (100);
    lcdPrint();
    if (debugEn == 1) {
      Serial.print("RAM Free = ");
      Serial.print(ramAvailMB );
      Serial.println(" MB");
      Serial.print("RAM Usage= ");
      Serial.print(ramAvailPercent);
      Serial.println(" %");
      Serial.println("===========================");
    }
    strMikrotikRAM = "";
  }
}

void reconnect() {
  if (WiFi.status() == WL_CONNECTED) {
    while (!client.connected()) {
      Serial.println("Menyambungkan ke MQTT broker");
      String clientId = "ESP8266Client-";
      clientId += String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) {
        Serial.println("MQTT broker tersambung");
        Serial.println("Alamat IP MQTT Broker : ");
        Serial.println(mqtt_server);
        
        client.subscribe("#");
      } else {
        Serial.print("Koneksi broker gagal, rc=");

        Serial.println(client.state());
        Serial.println("Menghubungkan kembali");
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Connecting To");
        lcd.setCursor(0,1);
        lcd.print(mqtt_server);
        delay(5000);
      }
    }
  }
}

void wifiSelectGUI() {
  // If not connected to a network, listen for incoming clients
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  Serial.println("New client");
  while (client.connected()) {
    if (client.available()) {
      String req = client.readStringUntil('\r');
      Serial.println(req);

      if (req.indexOf("/select") != -1) {
        int startIndex = req.indexOf("ssid=") + 5;
        int endIndex = req.indexOf("&", startIndex);
        int passStartIndex = req.indexOf("password=") + 9;
        int passEndIndex = req.indexOf("HTTP", passStartIndex);
        selectedSSID = req.substring(startIndex, endIndex);
        selectedPassword = req.substring(passStartIndex, passEndIndex - 1);
        int ssidLen = selectedSSID.length() + 1;
        int passLen = selectedPassword.length() + 1;
        char charSSID[ssidLen];
        char charPass[passLen];

        selectedSSID.toCharArray(charSSID, ssidLen);
        selectedPassword.toCharArray(charPass, passLen);

        Serial.print("REQ RAW = ");
        Serial.println(req);
        Serial.print("Selected SSID: '");
        Serial.print(selectedSSID);
        Serial.println("'");
        Serial.print("Selected password: '");
        Serial.print(selectedPassword);
        Serial.println("'");
        Serial.println("Clearing EEPROM");
        for (int i = 0 ; i < EEPROM.length() ; i++) {
          EEPROM.write(i, 0);
          Serial.println(i);
        }
        Serial.println("EEPROM Cleared");
        Serial.println("Writing SSID");

        for (int i = 0; i < 32; ++i) {
          if (i < selectedSSID.length()) {
            EEPROM.write(wifi_ssid_address + i, selectedSSID[i]);
            Serial.println(selectedSSID[i]);
            //            EEPROM.write(wifi_ssid_address + i, charSSID[i]);
          }
          else {
            EEPROM.write(wifi_ssid_address + i, 0);
          }
        }
        Serial.println("Writing PASSWORD");
        for (int i = 0; i < 32; ++i) {
          if (i < selectedPassword.length()) {
            EEPROM.write(wifi_password_address + i, charPass[i]);
          }
          else {
            EEPROM.write(wifi_password_address + i, 0);
          }
        }
        EEPROM.commit();
        ESP.restart();


        for (int i = 0; i < 32; ++i) {
          storedSSID += EEPROM.read(wifi_ssid_address + i);
          //          storedSSID += char(EEPROM.read(wifi_ssid_address + i));
        }
        for (int i = 0; i < 32; ++i) {
          storedPassword += char(EEPROM.read(wifi_password_address + i));
        }
        Serial.print("Stored SSID = "); Serial.println(storedSSID);
        Serial.print("Stored Pass = "); Serial.println(storedPassword);
      }

      client.flush();

      String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><head><title>WiFi Scan</title></head><body><h1>WiFi Networks</h1><form action='/select' method='get'><table><tr><th>SSID</th><th>Signal</th><th>Encryption</th><th>Select</th></tr>";
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; ++i) {
        s += "<tr><td>";
        s += WiFi.SSID(i);
        s += "</td><td>";
        s += WiFi.RSSI(i);
        s += " dBm</td><td>";
        s += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "Open" : "Encrypted";
        s += "</td><td>";
        s += "<input type='radio' name='ssid' value='" + WiFi.SSID(i) + "'>";
        s += "</td></tr>";
      }
      s += "</table><br><label for='password'>Password:</label><input type='text' id='password' name='password'><br><br><input type='submit' value='Select'></form></body></html>\r\n";

      client.print(s);
      break;
    }
  }
  client.stop();
  Serial.println("Client disconnected");
}

void resetBtn() {
  btn1.Update();
  if (btn1.clicks != 0) btnFunc = btn1.clicks;
  if (btnFunc == -1) {
    btnFunc = 0;
    Serial.println("Clearing EEPROM");
    for (int i = 0 ; i < EEPROM.length() ; i++) {
      EEPROM.write(i, 0);
      Serial.println(i);
    }
    EEPROM.commit();
    ESP.restart();
  }
}

void wifiCredentialCheck() {
  EEPROM.begin(64);
  delay(100);
  Serial.println();

  // Check if stored WiFi credentials are present
  for (int i = 0; i < 32; ++i) {
    storedSSID += char(EEPROM.read(wifi_ssid_address + i));
  }
  for (int i = 0; i < 32; ++i) {
    storedPassword += char(EEPROM.read(wifi_password_address + i));
  }
  Serial.print("Boot Stored SSID = "); Serial.println(storedSSID);
  Serial.print("Boot Stored Pass = "); Serial.println(storedPassword);

  // If stored WiFi credentials are present, attempt to connect to the network
  if (storedSSID != "" && storedPassword != "") {
    eepromStat = 0;
    WiFi.mode(WIFI_STA);
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && i < 10) {
      delay(1000);
      Serial.print("Connecting to stored WiFi network...");
      Serial.println(storedSSID);
      lcd.setCursor(0, 0);
      lcd.print("Connecting to");
      lcd.setCursor(0, 1);
      lcd.print(storedSSID.c_str());
      i++;
    }
  }
  // If the ESP8266 is not connected to a network, start an Access Point
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssidAp, passwordAp);

    server.begin();
    Serial.println("Server started");
    Serial.print("HotSpot IP address: ");
    Serial.println(WiFi.softAPIP());
    lcd.setCursor(0, 0);
    lcd.print("For setup go to");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.softAPIP());
    eepromStat = 1;
  }
  Serial.print("EEPROM STAT = ");
  Serial.println(eepromStat);
  //  Serial.print("IP ADDRESS = ");
  //  Serial.println(WiFi.localIP());
}

void stat() {
  if (eepromStat == 1) {
    interval = 250;
  }
  if (eepromStat == 0) {
    interval = 1000;
  }
  int displayAlter;

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(ledPin, LOW);
    delay(5);
    digitalWrite(ledPin, HIGH);
  }
}
