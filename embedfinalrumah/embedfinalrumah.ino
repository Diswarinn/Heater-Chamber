#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include "DHT.h"

// Konfigurasi WiFi
const char* ssid = "thor";
const char* password = "12345678";

// Pin DHT11
#define DHTPIN 4
#define DHTTYPE DHT11

// Relay pins
#define RELAY1 23  // FAN/Kipas
#define RELAY2 19  // HEATER

// Target suhu dan threshold
#define TARGET_TEMP 27.0
#define THRESHOLD 0.5  // 0.5°C untuk box 10cm (lebih stabil dari 0.3°C)

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x25, 16, 2);
WebServer server(80);

// Variabel global
float currentTemp = 0;
float currentHum = 0;
bool manualMode = false;
bool fanState = false;
bool heaterState = false;
unsigned long lastSensorRead = 0;
unsigned long lastLCDUpdate = 0;

// Status koneksi WiFi
bool wifiConnected = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  SISTEM STABILISASI SUHU RUANGAN  ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // Inisialisasi Relay (LOW = OFF untuk relay HIGH trigger)
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, LOW);  // FAN OFF
  digitalWrite(RELAY2, LOW);  // HEATER OFF
  Serial.println("✓ Relay dimatikan (standby)");
  
  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Stabilisasi");
  lcd.setCursor(0, 1);
  lcd.print("Suhu Ruangan");
  Serial.println("✓ LCD siap");
  
  // Inisialisasi DHT11
  dht.begin();
  delay(2000);
  Serial.println("✓ DHT11 sensor siap");
  
  // Koneksi WiFi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Koneksi WiFi...");
  Serial.print("\n→ Menghubungkan ke WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✓ WiFi terhubung!");
    Serial.print("→ IP Address: ");
    Serial.println(WiFi.localIP());
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi OK!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    
    // Setup Web Server
    setupWebServer();
    server.begin();
    Serial.println("✓ Web server aktif");
  } else {
    wifiConnected = false;
    Serial.println("\n✗ WiFi gagal terhubung");
    Serial.println("→ Sistem tetap jalan tanpa WiFi");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Gagal");
    lcd.setCursor(0, 1);
    lcd.print("Mode Lokal");
  }
  
  delay(2000);
  lcd.clear();
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║         SISTEM SIAP JALAN          ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.println("\nTarget Suhu  : 27.0°C");
  Serial.println("Threshold    : ±0.5°C");
  Serial.println("Range Stabil : 26.5°C - 27.5°C");
  Serial.println("────────────────────────────────────\n");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Handle web requests
  if (wifiConnected) {
    server.handleClient();
  }
  
  // Baca sensor setiap 2 detik
  if (currentMillis - lastSensorRead >= 2000) {
    lastSensorRead = currentMillis;
    readSensor();
    
    // Kontrol suhu otomatis jika tidak manual
    if (!manualMode) {
      controlTemperature();
    }
  }
  
  // Update LCD setiap 1 detik
  if (currentMillis - lastLCDUpdate >= 1000) {
    lastLCDUpdate = currentMillis;
    updateLCD();
  }
}

void readSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    Serial.println("⚠ Error membaca sensor DHT11!");
    return;
  }
  
  currentTemp = t;
  currentHum = h;
  
  Serial.println("┌─────────────────────────────────┐");
  Serial.print("│ Suhu       : ");
  Serial.print(currentTemp, 1);
  Serial.println(" °C");
  Serial.print("│ Kelembaban : ");
  Serial.print(currentHum, 1);
  Serial.println(" %");
  Serial.println("└─────────────────────────────────┘");
}

void controlTemperature() {
  float tempDiff = currentTemp - TARGET_TEMP;
  
  // PANAS (suhu > 27.5°C) → Kipas nyala
  if (tempDiff > THRESHOLD) {
    if (!fanState) {
      digitalWrite(RELAY1, HIGH);  // FAN ON
      digitalWrite(RELAY2, LOW);   // HEATER OFF
      fanState = true;
      heaterState = false;
      Serial.println("🌀 KIPAS NYALA (Ruangan panas)");
    }
  }
  // DINGIN (suhu < 26.5°C) → Heater nyala
  else if (tempDiff < -THRESHOLD) {
    if (!heaterState) {
      digitalWrite(RELAY1, LOW);   // FAN OFF
      digitalWrite(RELAY2, HIGH);  // HEATER ON
      fanState = false;
      heaterState = true;
      Serial.println("🔥 HEATER NYALA (Ruangan dingin)");
    }
  }
  // STABIL (26.5°C - 27.5°C) → Semua mati
  else {
    if (fanState || heaterState) {
      digitalWrite(RELAY1, LOW);   // FAN OFF
      digitalWrite(RELAY2, LOW);   // HEATER OFF
      fanState = false;
      heaterState = false;
      Serial.println("✓ SUHU STABIL - Semua mati");
    }
  }
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print("C ");
  
  // Status
  if (manualMode) {
    lcd.print("MAN");
  } else if (fanState) {
    lcd.print("FAN");
  } else if (heaterState) {
    lcd.print("HTR");
  } else {
    lcd.print("OK ");
  }
  lcd.print(" ");
  
  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print(currentHum, 1);
  lcd.print("% ");
  
  if (wifiConnected) {
    lcd.print("WiFi");
  } else {
    lcd.print("----");
  }
  lcd.print("  ");
}

void setupWebServer() {
  // Halaman utama
  server.on("/", handleRoot);
  
  // API endpoints
  server.on("/data", handleData);
  server.on("/control", handleControl);
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Dashboard Kontrol Suhu</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
    }
    .card {
      background: white;
      border-radius: 20px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
    }
    h1 {
      color: #667eea;
      text-align: center;
      margin-bottom: 30px;
      font-size: 2em;
    }
    .status-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
      margin-bottom: 30px;
    }
    .status-box {
      background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
      color: white;
      padding: 25px;
      border-radius: 15px;
      text-align: center;
    }
    .status-box.temp {
      background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
    }
    .status-box.humidity {
      background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%);
    }
    .status-label {
      font-size: 0.9em;
      opacity: 0.9;
      margin-bottom: 5px;
    }
    .status-value {
      font-size: 2.5em;
      font-weight: bold;
    }
    .control-section {
      margin-top: 20px;
    }
    .control-section h2 {
      color: #333;
      margin-bottom: 15px;
      font-size: 1.3em;
    }
    .button-group {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 10px;
    }
    button {
      padding: 15px 20px;
      border: none;
      border-radius: 10px;
      font-size: 1em;
      font-weight: bold;
      cursor: pointer;
      transition: all 0.3s;
      color: white;
    }
    button:hover {
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(0,0,0,0.2);
    }
    .btn-auto {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    }
    .btn-fan {
      background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
    }
    .btn-heater {
      background: linear-gradient(135deg, #fa709a 0%, #fee140 100%);
    }
    .btn-off {
      background: linear-gradient(135deg, #30cfd0 0%, #330867 100%);
    }
    .status-indicator {
      display: flex;
      align-items: center;
      justify-content: space-between;
      background: #f8f9fa;
      padding: 15px;
      border-radius: 10px;
      margin-top: 20px;
    }
    .indicator-item {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .led {
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #ccc;
    }
    .led.on {
      background: #4ade80;
      box-shadow: 0 0 10px #4ade80;
    }
    .info-box {
      background: #e0e7ff;
      padding: 15px;
      border-radius: 10px;
      margin-top: 20px;
      border-left: 4px solid #667eea;
    }
    .info-box p {
      color: #4338ca;
      line-height: 1.6;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h1>🌡️ Dashboard Kontrol Suhu</h1>
      
      <div class="status-grid">
        <div class="status-box temp">
          <div class="status-label">Suhu Ruangan</div>
          <div class="status-value" id="temp">--</div>
        </div>
        <div class="status-box humidity">
          <div class="status-label">Kelembaban</div>
          <div class="status-value" id="hum">--</div>
        </div>
      </div>

      <div class="status-indicator">
        <div class="indicator-item">
          <div class="led" id="led-fan"></div>
          <span>Kipas</span>
        </div>
        <div class="indicator-item">
          <div class="led" id="led-heater"></div>
          <span>Heater</span>
        </div>
        <div class="indicator-item">
          <strong id="mode-text">Mode: Auto</strong>
        </div>
      </div>

      <div class="control-section">
        <h2>⚙️ Kontrol Manual</h2>
        <div class="button-group">
          <button class="btn-auto" onclick="setMode('auto')">AUTO</button>
          <button class="btn-fan" onclick="setMode('fan')">KIPAS ON</button>
          <button class="btn-heater" onclick="setMode('heater')">HEATER ON</button>
        </div>
        <button class="btn-off" onclick="setMode('off')" style="width: 100%; margin-top: 10px;">MATIKAN SEMUA</button>
      </div>

      <div class="info-box">
        <p><strong>Target:</strong> 27.0°C ± 0.5°C</p>
        <p><strong>Otomatis:</strong> Kipas nyala jika >27.5°C, Heater nyala jika <26.5°C</p>
      </div>
    </div>
  </div>

  <script>
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('temp').textContent = data.temp.toFixed(1) + '°C';
          document.getElementById('hum').textContent = data.hum.toFixed(1) + '%';
          
          document.getElementById('led-fan').classList.toggle('on', data.fan);
          document.getElementById('led-heater').classList.toggle('on', data.heater);
          
          document.getElementById('mode-text').textContent = 
            'Mode: ' + (data.manual ? 'Manual' : 'Auto');
        })
        .catch(error => console.error('Error:', error));
    }

    function setMode(mode) {
      fetch('/control?mode=' + mode)
        .then(response => response.text())
        .then(data => {
          console.log(data);
          updateData();
        })
        .catch(error => console.error('Error:', error));
    }

    // Update setiap 2 detik
    setInterval(updateData, 2000);
    updateData();
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"temp\":" + String(currentTemp, 1) + ",";
  json += "\"hum\":" + String(currentHum, 1) + ",";
  json += "\"fan\":" + String(fanState ? "true" : "false") + ",";
  json += "\"heater\":" + String(heaterState ? "true" : "false") + ",";
  json += "\"manual\":" + String(manualMode ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleControl() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    
    if (mode == "auto") {
      manualMode = false;
      Serial.println("→ Mode: OTOMATIS");
      server.send(200, "text/plain", "Mode AUTO");
    }
    else if (mode == "fan") {
      manualMode = true;
      digitalWrite(RELAY1, HIGH);  // FAN ON
      digitalWrite(RELAY2, LOW);   // HEATER OFF
      fanState = true;
      heaterState = false;
      Serial.println("→ Mode: MANUAL - Kipas ON");
      server.send(200, "text/plain", "FAN ON");
    }
    else if (mode == "heater") {
      manualMode = true;
      digitalWrite(RELAY1, LOW);   // FAN OFF
      digitalWrite(RELAY2, HIGH);  // HEATER ON
      fanState = false;
      heaterState = true;
      Serial.println("→ Mode: MANUAL - Heater ON");
      server.send(200, "text/plain", "HEATER ON");
    }
    else if (mode == "off") {
      manualMode = true;
      digitalWrite(RELAY1, LOW);   // FAN OFF
      digitalWrite(RELAY2, LOW);   // HEATER OFF
      fanState = false;
      heaterState = false;
      Serial.println("→ Mode: MANUAL - Semua OFF");
      server.send(200, "text/plain", "ALL OFF");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}