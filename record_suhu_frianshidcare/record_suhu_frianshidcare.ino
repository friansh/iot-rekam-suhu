#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ShiftRegister74HC595.h>

#define DHTPIN  2

#define CONNECTING_WIFI   1
#define CONNECTING_HOST   2
#define ERROR_PUSH        5
#define ERROR_SENSOR      6
#define ERROR_FORMAT      9
#define ERROR_UPLOAD      8
#define ERROR_HOST        9
#define ALL_OK            0

const char* ssid        = "ay4mR4spi";
const char* password    = "akukamukita111417";
const char* host        = "192.168.18.12";
const char* username    = "friansh";
const char* device_code = "athid_5dd42856da41d5.00546746";

const int httpPort = 80;
const int retvDataInterval = 10000;

int hostConnFail_Count = 0;
const int hostConnFail_Threshold = 5;

DynamicJsonDocument doc(200);

DHT dht(DHTPIN, DHT11);
LiquidCrystal_I2C lcd(0x27, 16, 2);
ShiftRegister74HC595<1> sr(12, 13, 14);

void setup() {
  Serial.begin(115200);
  dht.begin();
  lcd.begin();

  // Turn on the blacklight and print a message.
  lcd.backlight();
  lcd.print("friash HI&TEMP");
  lcd.setCursor(0,1);
  lcd.print("Data Acq alpha0");

  delay(2000);

  Serial.println("\n\nfriansh Heat Index\nData Acquisition\nver alpha1.1");
    
  wifiConnect();
}


void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    lcd.clear();
    lcd.home();
    lcd.print("DHT11 ERROR!!");

    printNum(ERROR_SENSOR);
    delay(1000);
    return;
  }

  float hic = dht.computeHeatIndex(t, h, false);

  Serial.println("\n>>Indeks panas sekarang: " + String(hic) + " dan kelembaban: " + String(h));
  lcd.clear();
  lcd.home();
  lcd.print("HI: " + String(hic));
  lcd.setCursor(0,1);
  lcd.print("RH: " + String(h) + "%");

  pushData(username, "suhu rumah", String(hic));
  pushData(username, "kelembaban rumah", String(h));

  delay(retvDataInterval);
}

void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) WiFi.disconnect();
  Serial.print("\nConnecting to " + String(ssid));
  lcd.clear();
  lcd.home();
  lcd.print("Connecting to");
  lcd.setCursor(0,1);
  lcd.print(String(ssid));
  printNum(CONNECTING_WIFI);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected\nIP address: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.home();
  lcd.print("Connected, IP:");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  delay(2000);
  hostConnFail_Count = 0;
}

void pushData(String username, String listName, String data) {
  Serial.println("\nConnecting to " + String(host) + " port 80");
  printNum(CONNECTING_HOST);
  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed on attempt " + String(++hostConnFail_Count));
    printNum(ERROR_HOST);
    if ( hostConnFail_Count >= hostConnFail_Threshold ) {
      wifiConnect();
      return;
    }
    return;
  } else {
    hostConnFail_Count = 0;
  }

  // We now create a URI for the request
  String url = constructURL(username, listName, data);
  Serial.println("Requesting URL: " + url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "device_code: " + device_code + "\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();

  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  String json = "";
  while (client.available()) json += client.readStringUntil('\r');
  json = json.substring(json.indexOf("{"), json.indexOf("}") + 1);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    printNum(ERROR_FORMAT);
    return;
  }

  String stts = doc["status"];
  String desc = doc["description"];

  if ( stts == "sukses" ) {
    printNum(ALL_OK);
  } else {
    printNum(ERROR_PUSH);
  }

  Serial.println("-Status\t: " + stts + "\n-Deskripsi\t: " + desc);
}

String constructURL(String username, String listName, String data) {
  String url = "/friansh-datacenter-ci/push";
  url += "?u=" + urlencode(username);
  url += "&ln=" + urlencode(listName);
  url += "&d=" + urlencode(data);
  return url;
}

// Kode untuk konversi ke URL encoding
String urlencode(String str){
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}

unsigned char h2int(char c){
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}

void printNum(int num){
  sr.setAllLow();
  switch(num){
     case 0:
      sr.set(0, HIGH);
      sr.set(1, HIGH);
      sr.set(2, HIGH);
      sr.set(3, HIGH);
      sr.set(4, HIGH);
      sr.set(5, HIGH);
      break;
     case 1:
      sr.set(1, HIGH);
      sr.set(2, HIGH);
      break;
    case 2:
      sr.set(0, HIGH);
      sr.set(1, HIGH);
      sr.set(6, HIGH);
      sr.set(4, HIGH);
      sr.set(3, HIGH);
      break;
    case 3:
      sr.set(0, HIGH);
      sr.set(1, HIGH);
      sr.set(2, HIGH);
      sr.set(3, HIGH);
      sr.set(6, HIGH);
      break;
    case 4:
      sr.set(5, HIGH);
      sr.set(6, HIGH);
      sr.set(1, HIGH);
      sr.set(2, HIGH);
      break;
    case 5:
      sr.set(0, HIGH);
      sr.set(2, HIGH);
      sr.set(3, HIGH);
      sr.set(5, HIGH);
      sr.set(6, HIGH);
      break;
    case 6:
      sr.set(0, HIGH);
      sr.set(2, HIGH);
      sr.set(3, HIGH);
      sr.set(4, HIGH);
      sr.set(5, HIGH);
      sr.set(6, HIGH);
      break;
    case 7:
      sr.set(0, HIGH);
      sr.set(1, HIGH);
      sr.set(2, HIGH);
      break;
    case 8:
      sr.set(0, HIGH);
      sr.set(1, HIGH);
      sr.set(2, HIGH);
      sr.set(3, HIGH);
      sr.set(4, HIGH);
      sr.set(5, HIGH);
      sr.set(6, HIGH);
      break;
    case 9:
      sr.set(0, HIGH);
      sr.set(1, HIGH);
      sr.set(2, HIGH);
      sr.set(3, HIGH);
      sr.set(5, HIGH);
      sr.set(6, HIGH);
      break;
  }
}
