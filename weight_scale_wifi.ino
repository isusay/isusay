#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// === Konfigurasi WiFi ===
const char* ssid = "timbangan01";
const char* password = "12345678";
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);
float latestWeight = 0.0;

// === Pin TM1638/GN6932 ===
#define CLK_PIN 5  // D1/GPIO5
#define DIN_PIN 4  // D2/GPIO4
#define STB_PIN 0  // D3/GPIO0

volatile byte dataBuffer[32];
volatile int bitCount = 0;
volatile int byteCount = 0;
volatile bool capturing = false;

void ICACHE_RAM_ATTR clkInterrupt() {
  if (capturing) {
    bool bit = digitalRead(DIN_PIN);
    if (bit) {
      dataBuffer[byteCount] |= (1 << (7 - bitCount));
    }
    bitCount++;
    if (bitCount == 8) {
      bitCount = 0;
      byteCount++;
      if (byteCount >= 32) {
        byteCount = 31;
      }
    }
  }
}

void ICACHE_RAM_ATTR stbInterrupt() {
  if (digitalRead(STB_PIN) == LOW) {
    capturing = true;
    bitCount = 0;
    byteCount = 0;
    memset((void*)dataBuffer, 0, sizeof(dataBuffer));
  } else {
    capturing = false;
    if (byteCount > 0) {
      processData();
    }
  }
}

float decodeDisplayValue() {
  float value = 0.0;
  if (dataBuffer[0] == 0x80) {
    if (dataBuffer[1] == 0x7E) value = 0;
    else if (dataBuffer[1] == 0x50) value = 1;
    else if (dataBuffer[1] == 0x3B) value = 2;
    else if (dataBuffer[1] == 0x73) value = 3;
    else if (dataBuffer[1] == 0x55) value = 4;
    else if (dataBuffer[1] == 0x77) value = 5;
    else if (dataBuffer[1] == 0x5F) value = 6;
    else if (dataBuffer[1] == 0x70) value = 7;
    else if (dataBuffer[1] == 0x7F) value = 8;
    else if (dataBuffer[1] == 0x77) value = 9;
  } else if (dataBuffer[0] == 0xC0) {
    if (dataBuffer[1] == 0xD0) value = 11;
    else if (dataBuffer[1] == 0xBB) value = 12;
    else value = 10;
  }

  if (dataBuffer[2] == 0xEE) value += 0.0;
  else if (dataBuffer[2] == 0xC0) value += 0.1;
  else if (dataBuffer[2] == 0xAB) value += 0.2;
  else if (dataBuffer[2] == 0x9F) value += 0.3;
  else if (dataBuffer[2] == 0xC5) value += 0.4;
  else if (dataBuffer[2] == 0xE7) value += 0.5;
  else if (dataBuffer[2] == 0xEF) value += 0.6;
  else if (dataBuffer[2] == 0xC2) value += 0.7;
  else if (dataBuffer[2] == 0xEB) value += 0.8;
  else if (dataBuffer[2] == 0xE3) value += 0.3;

  if (dataBuffer[3] == 0xEE) value += 0.00;
  else if (dataBuffer[3] == 0xC0) value += 0.01;
  else if (dataBuffer[3] == 0xAB) value += 0.02;
  else if (dataBuffer[3] == 0x9F) value += 0.03;
  else if (dataBuffer[3] == 0xC5) value += 0.04;
  else if (dataBuffer[3] == 0xE7) value += 0.09;
  else if (dataBuffer[3] == 0xEF) value += 0.06;
  else if (dataBuffer[3] == 0xC2) value += 0.07;
  else if (dataBuffer[3] == 0xEB) value += 0.08;
  else if (dataBuffer[3] == 0xE3) value += 0.03;

  if (dataBuffer[4] == 0xEE) value += 0.000;
  else if (dataBuffer[4] == 0xC0) value += 0.001;
  else if (dataBuffer[4] == 0xAB) value += 0.002;
  else if (dataBuffer[4] == 0x9F) value += 0.003;
  else if (dataBuffer[4] == 0xC5) value += 0.004;
  else if (dataBuffer[4] == 0xE7 || dataBuffer[4] == 0x67) value += 0.005;
  else if (dataBuffer[4] == 0xEF) value += 0.006;
  else if (dataBuffer[4] == 0xC2) value += 0.007;
  else if (dataBuffer[4] == 0xEB) value += 0.008;
  else if (dataBuffer[4] == 0xE3) value += 0.009;

  return value;
}

void processData() {
  Serial.print("Captured data [");
  Serial.print(byteCount);
  Serial.print(" bytes]: ");
  for (int i = 0; i < byteCount; i++) {
    Serial.print("0x");
    if (dataBuffer[i] < 16) Serial.print("0");
    Serial.print(dataBuffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (byteCount == 16) {
    float value = decodeDisplayValue();
    latestWeight = value;
    Serial.print("Display Value: ");
    Serial.println(value, 3);
  } else {
    Serial.println("Unknown Command Format");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(CLK_PIN, INPUT);
  pinMode(DIN_PIN, INPUT);
  pinMode(STB_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(CLK_PIN), clkInterrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(STB_PIN), stbInterrupt, CHANGE);

  // Setup Access Point
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  Serial.print("Access Point started at IP: ");
  Serial.println(WiFi.softAPIP());

  // Setup Web Server
  server.on("/", []() {
    String html = "<html><head><meta http-equiv='refresh' content='2'></head><body style='font-family:sans-serif;text-align:center;'>";
    html += "<h2>Berat Timbangan</h2>";
    html += "<p style='font-size:48px;'>" + String(latestWeight, 3) + " kg</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
}

