#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>

// WiFi Credentials
const char* ssid = "your wifi name";  //enter your wifi name here
const char* password = "your wifi password";  //enter your wifi password here

// I2C LCD Configuration
LiquidCrystal_I2C lcd(0x27, 20, 4);

// HamAlert Telnet Configuration
const char* telnetHost = "hamalert.org";
const int telnetPort = 7300;
const char* telnetUsername = "your callsign";  //enter your HamAlert username, usually your callsign
const char* telnetPassword = "your password";  //enter your HamAlert Password, usually defined in the phone app under telnet server password. 
  WiFiClient client;

// Struct for DX spots
struct DXSpot {
  String call;
  String freq;
  String spotter;
  String time;
  int cqZone;
};

DXSpot spots[4];
int spotCount = 0;

// Reconnect timing
unsigned long lastReconnectTime = 0;
const unsigned long reconnectInterval = 1800000; // 30 minutes in milliseconds

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  connectToWiFi();
  connectToTelnet();
  lastReconnectTime = millis(); // Initialize reconnect timer
}

void loop() {
  // Check WiFi status
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Disconnected");
    Serial.println("WiFi Disconnected. Reconnecting...");
    connectToWiFi();
    connectToTelnet();
    lastReconnectTime = millis();
  } 
  // Check Telnet connection and time for reconnect
  else if (!client.connected() || (millis() - lastReconnectTime >= reconnectInterval)) {
    client.stop(); // Silently disconnect
    connectToTelnetSilent(); // Reconnect without LCD updates
    lastReconnectTime = millis(); // Reset timer
  } 
  else {
    readTelnetSpots();
  }
  delay(1000);
}

void connectToWiFi() {
  WiFi.disconnect();
  delay(1000);
  WiFi.begin(ssid, password);
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi  ");
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print("...");
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(1000);
    Serial.print(".");
    lcd.setCursor(0, 1);
    lcd.print("Attempt " + String(counter + 1) + "    ");
    counter++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    delay(2000);
    displaySpots(); // Restore spots after WiFi connect
  } else {
    lcd.clear();
    lcd.print("WiFi Failed");
    Serial.println("\nFailed to connect to WiFi!");
  }
}

void connectToTelnet() {
  if (WiFi.status() != WL_CONNECTED) return;
  lcd.setCursor(0, 0);
  lcd.print("Connecting Telnet");
  Serial.print("Connecting to ");
  Serial.print(telnetHost);
  Serial.print(":" + String(telnetPort) + "...");

  int attempts = 0;
  while (attempts < 3 && !client.connect(telnetHost, telnetPort)) {
    Serial.println("Failed! Retry " + String(attempts + 1) + "/3");
    lcd.setCursor(0, 1);
    lcd.print("Retry " + String(attempts + 1) + "    ");
    delay(2000);
    attempts++;
  }

  if (client.connected()) {
    Serial.println("\nTelnet Connected!");
    lcd.setCursor(0, 0);
    lcd.print("Telnet Connected ");

    // Wait for login prompt
    int timeout = 0;
    while (!client.available() && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    if (client.available()) {
      String prompt = client.readStringUntil('\n');
      Serial.println("Server says: " + prompt);
    }
    client.println(telnetUsername); // Send "ZS6WAR"
    Serial.println("Sent username: " + String(telnetUsername));

    // Wait for password prompt
    timeout = 0;
    while (!client.available() && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    if (client.available()) {
      String prompt = client.readStringUntil('\n');
      Serial.println("Server says: " + prompt);
    }
    client.println(telnetPassword); // Send "jobjob"
    Serial.println("Sent password: " + String(telnetPassword));

    // Wait for login confirmation
    timeout = 0;
    while (!client.available() && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    if (client.available()) {
      String response = client.readStringUntil('\n');
      Serial.println("Server response: " + response);
      if (response.indexOf("invalid") == -1 && response.indexOf("failed") == -1) {
        lcd.clear();
        lcd.print("Telnet Ready");
        Serial.println("Login successful!");
        delay(2000);
        client.println("sh/dx 4"); // Request last 4 spots
        Serial.println("Requested last 4 spots with: sh/dx 4");
        displaySpots(); // Restore spots after initial connect
      } else {
        lcd.clear();
        lcd.print("Login Failed");
        Serial.println("Login failed!");
      }
    } else {
      lcd.clear();
      lcd.print("Telnet No Response");
      Serial.println("No response after login attempt!");
    }
  } else {
    lcd.clear();
    lcd.print("Telnet Failed");
    Serial.println("\nTelnet Connection Failed after 3 attempts!");
  }
}

void connectToTelnetSilent() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.print("Silently reconnecting to ");
  Serial.print(telnetHost);
  Serial.print(":" + String(telnetPort) + "...");

  int attempts = 0;
  while (attempts < 3 && !client.connect(telnetHost, telnetPort)) {
    Serial.println("Failed! Retry " + String(attempts + 1) + "/3");
    delay(2000);
    attempts++;
  }

  if (client.connected()) {
    Serial.println("\nTelnet Reconnected!");
    // Wait for login prompt
    int timeout = 0;
    while (!client.available() && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    if (client.available()) {
      String prompt = client.readStringUntil('\n');
      Serial.println("Server says: " + prompt);
    }
    client.println(telnetUsername); // Send "ZS6WAR"
    Serial.println("Sent username: " + String(telnetUsername));

    // Wait for password prompt
    timeout = 0;
    while (!client.available() && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    if (client.available()) {
      String prompt = client.readStringUntil('\n');
      Serial.println("Server says: " + prompt);
    }
    client.println(telnetPassword); // Send "jobjob"
    Serial.println("Sent password: " + String(telnetPassword));

    // Wait for login confirmation
    timeout = 0;
    while (!client.available() && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    if (client.available()) {
      String response = client.readStringUntil('\n');
      Serial.println("Server response: " + response);
      if (response.indexOf("invalid") == -1 && response.indexOf("failed") == -1) {
        Serial.println("Silent login successful!");
        client.println("sh/dx 4"); // Request last 4 spots
        Serial.println("Requested last 4 spots with: sh/dx 4");
      } else {
        Serial.println("Silent login failed!");
      }
    } else {
      Serial.println("No response after silent login attempt!");
    }
  } else {
    Serial.println("\nSilent Telnet Reconnection Failed after 3 attempts!");
  }
}

void readTelnetSpots() {
  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println("Spot: " + line);
    parseSpot(line);
    displaySpots();
  }
}

void parseSpot(String line) {
  // Skip non-DX lines
  if (line.indexOf("DX de ") == -1) return;

  // Parse format: "DX de SPOTTER:     FREQ  DX_CALL      SNR SPEED                      TIME"
  int dePos = line.indexOf("DX de ");
  if (dePos == -1) return;
  int colonPos = line.indexOf(":", dePos + 6);
  if (colonPos == -1) return;

  // Extract spotter (between "DX de " and ":")
  String spotter = line.substring(dePos + 6, colonPos);
  spotter.trim();

  // Move past colon and spaces to find freq
  int freqStart = colonPos + 1;
  while (freqStart < line.length() && line.charAt(freqStart) == ' ') freqStart++;
  int freqEnd = line.indexOf(" ", freqStart);
  if (freqEnd == -1) return;
  String freq = line.substring(freqStart, freqEnd);
  freq.trim();

  // Move past freq and spaces to find call
  int callStart = freqEnd + 1;
  while (callStart < line.length() && line.charAt(callStart) == ' ') callStart++;
  int callEnd = line.indexOf(" ", callStart);
  if (callEnd == -1) return;
  String call = line.substring(callStart, callEnd);
  call.trim();

  // Time is after the last space
  int timePos = line.lastIndexOf(" ");
  String time = line.substring(timePos + 1);
  time.trim();

  // Debug parsed values
  Serial.println("Parsed - spotter: " + spotter + ", freq: " + freq + 
                 ", call: " + call + ", time: " + time);

  // Check for CW via "wpm"
  if (line.indexOf("wpm") == -1) return;

  // Default CQ Zone
  int cqZone = 0;

  // Shift spots down, add new one at top
  for (int i = 3; i > 0; i--) {
    spots[i] = spots[i - 1];
  }
  spots[0] = {call, freq, spotter, time, cqZone};
  spotCount = min(spotCount + 1, 4);
  Serial.println("Added: " + time + " " + call + " " + freq + " by " + spotter);
}

void displaySpots() {
  lcd.clear();
  for (int i = 0; i < spotCount; i++) {
    // Calculate extra spaces based on callsign length (max 6)
    int callLength = spots[i].call.length();
    int extraSpaces = (callLength < 6) ? (6 - callLength) : 0;
    String spacing = " "; // Default single space
    for (int j = 0; j < extraSpaces; j++) {
      spacing += " "; // Add one space per character less than 6
    }

    // Build the display line with dynamic spacing
    String line = spots[i].time + " " + spots[i].call + spacing + spots[i].freq;

    Serial.println("Displaying row " + String(i) + ": time=" + spots[i].time + 
                   ", call=" + spots[i].call + ", freq=" + spots[i].freq + 
                   ", spaces=" + String(extraSpaces));
    lcd.setCursor(0, i);
    lcd.print(line.substring(0, 20));
  }
}
