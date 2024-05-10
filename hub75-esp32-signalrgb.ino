#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// Network settings
const char* ssid = "YOUR SSID";
const char* password = "YOUR PASS";
const int udpPort = 1234;


// Matrix configuration
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANELS_NUMBER 1

// Global objects
WiFiUDP udp;
MatrixPanel_I2S_DMA *matrix = nullptr;
WebServer server(80);

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// LED settings
const int NUM_LEDS = 2048;
RGB rgbBuffer[NUM_LEDS];
int currentBrightness = 60;
static int totalLEDsUpdated = 0; // Keep track of the total number of LEDs updated

// Function declarations
void setupWiFi();
void setupMDNS();
void setupUDP();
void setupWebServer();
void handleDiscoveryRequest();
void handleRGBData();
void handleDeviceStateChange();
void handleDiscoveryGETRequest();
void setPanelBrightness(int brightness);
int getPanelBrightness();


void setup() {
  Serial.begin(115200);
  setupWiFi();
  setupMDNS();
  setupUDP();
  setupWebServer();

  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANELS_NUMBER    // Chain length
  );

  matrix = new MatrixPanel_I2S_DMA(mxconfig);
  matrix->begin();
  setPanelBrightness(currentBrightness);
  matrix->clearScreen();
}

void loop() {
  server.handleClient();
  handleDiscoveryRequest();
}

void setupWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMDNS() {
  if (!MDNS.begin("esp32-hub75")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started");
    MDNS.addService("_hub75", "_tcp", 80);
  }
}

void setupUDP() {
  udp.begin(udpPort);
  Serial.print("Listening on UDP port ");
  Serial.println(udpPort);
}

void setupWebServer() {
  server.on("/json/", HTTP_GET, handleFullDeviceInfoRequest);
  server.on("/json/info/", HTTP_GET, handleDiscoveryGETRequest);
  server.on("/json/state/", HTTP_POST, handleDeviceStateChange);
  server.begin();
}

void handleDiscoveryRequest() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packetBuffer[1500];
    int len = udp.read(packetBuffer, sizeof(packetBuffer));
    packetBuffer[len] = 0;

    String incomingPacket = String(packetBuffer);
    if (incomingPacket.startsWith("DISCOVER_REQUEST")) {
      DynamicJsonDocument doc(1024);
      doc["ip"] = WiFi.localIP().toString();
      doc["mac"] = WiFi.macAddress();
      doc["type"] = "WLED";
      doc["name"] = "ESP32-HUB75";

      String response;
      serializeJson(doc, response);

      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.write((uint8_t*)response.c_str(), response.length());
      udp.endPacket();
      Serial.println("Sent discovery response");
    } else {
      handleRGBData(packetBuffer, len);
    }
  }
}

void handleRGBData(char* data, int length) {
    int startingIndex = (data[2]<<8) | data[3];
    int numLEDs = (length - 4) / 3; // Calculate the number of LEDs (each LED requires 3 bytes for RGB)
    int byteOffset = 4;
    for (int i = 0; i < numLEDs; i++) {
        // Calculate the index for the RGB values in the data array
        
        int r = data[i * 3 + byteOffset];
        int g = data[i * 3 + byteOffset + 1];
        int b = data[i * 3 + byteOffset + 2];

        if (totalLEDsUpdated < NUM_LEDS) {
          rgbBuffer[startingIndex + i] = {r, g, b};
          totalLEDsUpdated++;
        }
        
        if (totalLEDsUpdated >= NUM_LEDS || startingIndex + i >= NUM_LEDS) {
        // Draw all pixels
        for (int i = 0; i < NUM_LEDS; i++) {
            int x = i % matrix->width();
            int y = i / matrix->width();
            matrix->drawPixelRGB888(x, y, rgbBuffer[i].r, rgbBuffer[i].g, rgbBuffer[i].b);
        }
        totalLEDsUpdated = 0; // Reset buffer index for new data
        return;
      }
    }
}

void handleDeviceStateChange() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Bad Request: Body not received");
    return;
  }

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, server.arg("plain"));
  bool on = doc["on"];
  int bri = doc["bri"];

  if (on) {
    matrix->fillScreen(matrix->color565(255, 255, 255));
  } else {
    matrix->fillScreen(matrix->color565(0, 0, 0));
  }
  setPanelBrightness(bri);
  server.send(200, "text/plain", "Device state updated");
}

void handleDiscoveryGETRequest() { //on /json/info/
 
  DynamicJsonDocument doc(512); // Adjust size based on actual needs

  // Device and firmware information
  doc["brand"] = "WLED";  // Brand to verify it's a WLED device
  doc["name"] = "HUB75-Panel";  // Device name
  doc["mac"] = WiFi.macAddress();  // MAC address
  doc["ip"] = WiFi.localIP().toString();  // IP address

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json",response);
}

void handleFullDeviceInfoRequest() { // on /json/
  DynamicJsonDocument doc(1024);  // Adjust size based on actual needs

  // State information
  JsonObject state = doc.createNestedObject("state");
  state["on"] = (currentBrightness > 0);  // Assuming brightness > 0 means ON
  state["bri"] = currentBrightness;       // Current brightness level

  // Device and firmware information
  JsonObject info = doc.createNestedObject("info");
  info["udpport"] = udpPort;  // UDP port for additional network services
  info["arch"] = "ESP32";     // Architecture
  info["ver"] = "1.0.0";      // Firmware version

  // LED information
  JsonObject leds = info.createNestedObject("leds");
  leds["count"] = PANEL_RES_X * PANEL_RES_Y;  // Total number of LEDs

  // WiFi information
  JsonObject wifi = info.createNestedObject("wifi");
  wifi["signal"] = WiFi.RSSI();  // WiFi signal strength

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void setPanelBrightness(int brightness) {
    matrix->setBrightness8(brightness);
    currentBrightness = brightness;  // Update the stored brightness level
}

int getPanelBrightness() {
    return currentBrightness * 100 / 255;  // Return the stored brightness level
}
