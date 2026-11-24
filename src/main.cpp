#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiSetup.h>

WiFiSetup wm;
ESP8266WebServer server(80);

const char AP_NAME[] = "ESP-OnDemand-Setup";
const char AP_PASS[] = "12345678";

// Одноканальне реле (активний низький рівень LOW)
const int relay1 = D2; // GPIO 4
bool relay1State = false;
bool serverStarted = false;

enum States
{
  CONNECTING,
  SUCCESS,
  FAILED,
};

States cur_state = CONNECTING;

unsigned long last_time_connecting = 0;
unsigned long last_time = 0;
unsigned long start_attempt_time = 0;
unsigned long failed_entered_time = 0;
const unsigned long connect_timeout = 30000;
const unsigned long restart_delay = 10000;

void handleRoot()
{
  String html = "<!DOCTYPE html><html lang='uk'>";
  html += "<head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Smart Home | 1 Channel</title>";
  html += "<style>";
  html += "*{box-sizing:border-box;margin:0;padding:0;}";
  html += "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#eaf2ff,#dce5f2);color:#172033;min-height:100vh;padding:20px;display:flex;align-items:center;justify-content:center;}";
  html += ".card{width:100%;max-width:420px;background:white;border-radius:16px;padding:28px 22px;text-align:center;box-shadow:0 8px 24px rgba(20,45,85,0.1);}";
  html += "h2{color:#183b70;margin-bottom:6px;}";
  html += ".pin{font-size:13px;color:#7b8798;margin-bottom:20px;}";
  html += ".badge{display:inline-block;padding:6px 16px;border-radius:20px;font-size:12px;font-weight:bold;margin-bottom:22px;}";
  html += ".on{background:#d9f8e6;color:#08783c;}";
  html += ".off{background:#ffe1e4;color:#c6283d;}";
  html += ".btn{display:inline-block;padding:13px 28px;border-radius:10px;text-decoration:none;color:white;font-size:15px;font-weight:bold;}";
  html += ".btn-on{background:#1769d2;}";
  html += ".btn-off{background:#c5223d;}";
  html += ".footer{margin-top:22px;font-size:11px;color:#8892a0;}";
  html += "</style></head><body>";

  html += "<div class='card'>";
  html += "<h2>Канал 01 (Реле)</h2>";
  html += "<div class='pin'>GPIO 4 | D2 (Active LOW)</div>";

  if (relay1State)
  {
    html += "<div class='badge on'>СТАТУС: УВІМКНЕНО (ON)</div><br>";
    html += "<a class='btn btn-off' href='/r1/off'>Вимкнути</a>";
  }
  else
  {
    html += "<div class='badge off'>СТАТУС: ВИМКНЕНО (OFF)</div><br>";
    html += "<a class='btn btn-on' href='/r1/on'>Увімкнути</a>";
  }

  html += "<div class='footer'>Wemos D1 mini • Non-Blocking Web Server</div>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleR1On()
{
  relay1State = true;
  digitalWrite(relay1, LOW); // Активний низький рівень
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleR1Off()
{
  relay1State = false;
  digitalWrite(relay1, HIGH); // Вимкнення реле
  server.sendHeader("Location", "/");
  server.send(303);
}

void startWebServerOnce()
{
  if (!serverStarted)
  {
    server.on("/", handleRoot);
    server.on("/r1/on", handleR1On);
    server.on("/r1/off", handleR1Off);
    server.begin();
    serverStarted = true;
    Serial.printf("HTTP server started at http://%s\n", WiFi.localIP().toString().c_str());
  }
}

void printHelp()
{
  Serial.println("\nSerial Commands:");
  Serial.println(" 'p' - Toggle Config Portal on / off");
  Serial.println(" 'r' - Reset Wi-Fi credentials");
  Serial.println(" 's' - Print connection status");
  Serial.println(" 'h' - Print help");
}

void togglePortal()
{
  if (wm.getConfigPortalActive())
  {
    Serial.println("Stopping Config Portal...");
    wm.stopConfigPortal();
    if (wm.getWiFiIsSaved())
    {
      Serial.printf("Reconnecting to saved SSID: %s\n", wm.getWiFiSSID().c_str());
      WiFi.begin(wm.getWiFiSSID().c_str(), wm.getWiFiPass().c_str());
    }
  }
  else
  {
    Serial.println("Starting Config Portal on demand...");
    if (WiFi.status() == WL_CONNECTED)
    {
      WiFi.disconnect();
    }
    wm.startConfigPortal(AP_NAME, AP_PASS);
    Serial.printf("Config Portal active at http://192.168.4.1 (AP: %s)\n", AP_NAME);
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(relay1, OUTPUT);
  digitalWrite(relay1, HIGH); // Початковий стан: вимкнено (Active LOW)

  wm.setWiFiAutoReconnect(true);
  wm.setEnableConfigPortal(false);

  unsigned long now = millis();
  start_attempt_time = now;
  last_time_connecting = now;
  last_time = now;

  Serial.println("\n--- Wemos D1 mini 1-Channel Relay + WiFiSetup ---");
  printHelp();
}

void loop()
{
  unsigned long cur_time = millis();

  switch (cur_state)
  {
  case CONNECTING:
    if (cur_time - last_time_connecting >= 1000)
    {
      last_time_connecting = cur_time;

      bool isConnected = wm.autoConnect(15);
      if (isConnected)
      {
        Serial.printf("\nConnected to %s! IP: %s\n",
                      wm.getWiFiSSID().c_str(),
                      WiFi.localIP().toString().c_str());
        startWebServerOnce();
        last_time = cur_time;
        cur_state = SUCCESS;
      }
      else if (cur_time - start_attempt_time >= connect_timeout)
      {
        Serial.println("\nConnection failed or no credentials saved.");
        togglePortal();
        last_time = cur_time;
        cur_state = SUCCESS;
      }
      else
      {
        Serial.print(".");
      }
    }
    break;

  case SUCCESS:
    if (WiFi.status() == WL_CONNECTED)
    {
      startWebServerOnce();
      server.handleClient();
    }

    if (cur_time - last_time >= 20)
    {
      last_time = cur_time;
      wm.process();

      if (Serial.available())
      {
        char cmd = Serial.read();
        if (cmd == 'p')
        {
          togglePortal();
        }
        else if (cmd == 'r')
        {
          Serial.println("Erasing saved Wi-Fi settings...");
          wm.resetSettings();
        }
        else if (cmd == 's')
        {
          Serial.println("\n--- Status ---");
          Serial.printf("Relay 1: %s\n", relay1State ? "ON" : "OFF");
          Serial.printf("Connected: %s\n", (WiFi.status() == WL_CONNECTED) ? "YES" : "NO");
          Serial.printf("Portal active: %s\n", wm.getConfigPortalActive() ? "YES" : "NO");
          Serial.printf("Saved SSID: %s\n", wm.getWiFiSSID().c_str());
          if (WiFi.status() == WL_CONNECTED)
          {
            Serial.printf("IP: %s (RSSI: %d dBm)\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
          }
          Serial.println("--------------");
        }
        else if (cmd == 'h')
        {
          printHelp();
        }
      }
    }
    break;

  case FAILED:
    if (failed_entered_time == 0)
    {
      failed_entered_time = cur_time;
    }
    if (cur_time - failed_entered_time >= restart_delay)
    {
      Serial.println("Restarting ESP8266...");
      ESP.restart();
    }
    break;
  }
}