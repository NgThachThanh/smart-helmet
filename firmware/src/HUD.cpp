#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define OLED_SDA 33
#define OLED_SCL 32
#define OLED_RESET -1

// =====================================================
// BLE Configuration
// =====================================================
#define SERVICE_UUID "DD3F0AD1-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_INDICATE_UUID "DD3F0AD2-6239-4E1F-81F1-91F6C9F01D86"
#define CHAR_WRITE_UUID "DD3F0AD3-6239-4E1F-81F1-91F6C9F01D86"

// =====================================================
// OLED Configuration
// =====================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LOGO_HEIGHT 32
#define LOGO_WIDTH 32

// =====================================================
// Navigation Data Structure - Updated for React Native Format
// =====================================================
struct NavigationInfo {
  String currentStreet = "";
  String nextDirection = "";
  uint32_t distanceToTurn = 0;
  String nextStreet = "";
  bool isNavigating = false;
  String currentInstruction = "";
  String nextInstruction = "";
  int stepIndex = 0;
  int totalSteps = 0;
};

// =====================================================
// Global Variables
// =====================================================
BLEServer* g_pServer = NULL;
BLECharacteristic* g_pCharIndicate = NULL;
bool g_deviceConnected = false;
uint32_t g_lastActivityTime = 0;
bool g_isNaviDataUpdated = false;
String g_receivedData = "";
NavigationInfo g_navInfo;

String g_deviceName = "";
String g_deviceId = "";

unsigned long lastDataReceived = 0;
unsigned long totalDataReceived = 0;
unsigned long successfulParses = 0;
unsigned long failedParses = 0;

// =====================================================
// Direction Icons (32x32 bitmap)
// =====================================================
const unsigned char TRAI[] PROGMEM = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 
0x00, 0x07, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 
0x00, 0xFC, 0x00, 0x00, 0x01, 0xFC, 0x00, 0x00, 0x03, 0xFF, 0xFC, 0x00, 0x07, 0xFF, 0xFE, 0x00, 
0x03, 0xFF, 0xFF, 0x00, 0x00, 0xFC, 0x0F, 0x80, 0x00, 0x7C, 0x03, 0xC0, 0x00, 0x3E, 0x01, 0xC0,
0x00, 0x0E, 0x01, 0xE0, 0x00, 0x07, 0x00, 0xE0, 0x00, 0x03, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 
0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 
0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0xE0, 
0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char PHAI[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x3E, 0x00,
  0x00, 0x00, 0x3F, 0x80, 0x01, 0xFF, 0xFF, 0xC0, 0x07, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xFC,
  0x1F, 0xFF, 0xFF, 0xFE, 0x3F, 0xFF, 0xFF, 0xFE, 0x3F, 0xFF, 0xFF, 0xF8, 0x7F, 0xFF, 0xFF, 0xE0,
  0x7F, 0xFF, 0xFF, 0xC0, 0x7F, 0xC0, 0x3F, 0x00, 0x7F, 0x80, 0x3C, 0x00, 0x7F, 0x80, 0x38, 0x00,
  0x7F, 0x80, 0x20, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00,
  0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00,
  0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00,
  0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00, 0x7F, 0x80, 0x00, 0x00
};

const unsigned char THANG[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xC0, 0x00, 0x00, 0x03, 0xE0, 0x00, 0x00, 0x07, 0xF0, 0x00,
  0x00, 0x0F, 0xF8, 0x00, 0x00, 0x1F, 0xFC, 0x00, 0x00, 0x3F, 0xFE, 0x00, 0x00, 0x7F, 0xFF, 0x00,
  0x00, 0xFF, 0xFF, 0x80, 0x01, 0xFF, 0xFF, 0x80, 0x01, 0xFF, 0xFF, 0x80, 0x00, 0xFF, 0xFF, 0x00,
  0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00,
  0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00,
  0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00,
  0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00,
  0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char POINT[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x3F, 0xF8, 0x00,
  0x00, 0x7F, 0xFC, 0x00, 0x00, 0x78, 0x1E, 0x00, 0x00, 0xF0, 0x0E, 0x00, 0x00, 0xE0, 0x0F, 0x00,
  0x01, 0xE0, 0x07, 0x00, 0x01, 0xC0, 0x07, 0x00, 0x01, 0xC0, 0x07, 0x00, 0x01, 0xE0, 0x07, 0x00,
  0x00, 0xE0, 0x0F, 0x00, 0x00, 0xF0, 0x0E, 0x00, 0x00, 0x78, 0x3E, 0x00, 0x00, 0x7F, 0xFC, 0x00,
  0x00, 0x3F, 0xFC, 0x00, 0x00, 0x3F, 0xF8, 0x00, 0x00, 0x1F, 0xF0, 0x00, 0x00, 0x1F, 0xF0, 0x00,
  0x00, 0x0F, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xC0, 0x00, 0x00, 0x03, 0xC0, 0x00,
  0x00, 0x03, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0F, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00
};

// =====================================================
// Direction Code Converter
// =====================================================
String convertDirectionCode(int dirCode) {
  switch (dirCode) {
    case 0: return "THANG";
    case 1: return "TRAI";
    case 2: return "PHAI";
    case 3: return "QUAY DAU";
    case 4: return "DEN DICH";
    default: return "TIEP TUC";
  }
}

// =====================================================
// Vietnamese Character Converter (diacritics removal for SSD1306)
// =====================================================
String convertVietnameseToDisplay(const String& input) {
  String output = "";
  output.reserve(input.length() * 2);

  for (int i = 0; i < input.length(); i++) {
    unsigned char c = (unsigned char)input.charAt(i);

    if (c <= 0x7F) {
      if (c >= 32 && c <= 126) {
        output += (char)c;
      }
      continue;
    }

    if (c >= 0xC0 && c <= 0xDF && i + 1 < input.length()) {
      unsigned char c2 = (unsigned char)input.charAt(i + 1);

      if ((c2 & 0xC0) != 0x80) {
        continue;
      }

      uint16_t utf8Code = ((c & 0x1F) << 6) | (c2 & 0x3F);

      switch (utf8Code) {
        case 0x00C0: case 0x00C1: case 0x00C3:
        case 0x00C2:
          output += "A"; break;
        case 0x1EA0: case 0x1EA2:
        case 0x1EA6: case 0x1EA4: case 0x1EA8: case 0x1EAA: case 0x1EAC:
        case 0x0102: case 0x1EB0: case 0x1EAE: case 0x1EB2: case 0x1EB4: case 0x1EB6:
          output += "A"; break;
        case 0x00E0: case 0x00E1: case 0x00E3:
        case 0x00E2:
          output += "a"; break;
        case 0x1EA1: case 0x1EA3:
        case 0x1EA7: case 0x1EA5: case 0x1EA9: case 0x1EAB: case 0x1EAD:
        case 0x0103: case 0x1EB1: case 0x1EAF: case 0x1EB3: case 0x1EB5: case 0x1EB7:
          output += "a"; break;
        case 0x00C8: case 0x00C9: case 0x1EB8: case 0x1EBA: case 0x1EBC:
        case 0x00CA: case 0x1EC0: case 0x1EBE: case 0x1EC2: case 0x1EC4: case 0x1EC6:
          output += "E"; break;
        case 0x00E8: case 0x00E9: case 0x1EB9: case 0x1EBB: case 0x1EBD:
        case 0x00EA: case 0x1EC1: case 0x1EBF: case 0x1EC3: case 0x1EC5: case 0x1EC7:
          output += "e"; break;
        case 0x00CC: case 0x00CD: case 0x1CA: case 0x1EC8: case 0x0128:
          output += "I"; break;
        case 0x00EC: case 0x00ED: case 0x1ECB: case 0x1EC9: case 0x0129:
          output += "i"; break;
        case 0x00D2: case 0x00D3: case 0x1ECC: case 0x1ECE: case 0x00D5:
        case 0x00D4: case 0x1ED2: case 0x1ED0: case 0x1ED4: case 0x1ED6: case 0x1ED8:
        case 0x01A0: case 0x1EDC: case 0x1EDA: case 0x1EDE: case 0x1EE0: case 0x1EE2:
          output += "O"; break;
        case 0x00F2: case 0x00F3: case 0x1ECD: case 0x1ECF: case 0x00F5:
        case 0x00F4: case 0x1ED3: case 0x1ED1: case 0x1ED5: case 0x1ED7: case 0x1ED9:
        case 0x01A1: case 0x1EDD: case 0x1EDB: case 0x1EDF: case 0x1EE1: case 0x1EE3:
          output += "o"; break;
        case 0x00D9: case 0x00DA: case 0x1EE4: case 0x1EE6: case 0x0168:
        case 0x01AF: case 0x1EEA: case 0x1EE8: case 0x1EEC: case 0x1EEE: case 0x1EF0:
          output += "U"; break;
        case 0x00F9: case 0x00FA: case 0x1EE5: case 0x1EE7: case 0x0169:
        case 0x01B0: case 0x1EEB: case 0x1EE9: case 0x1EB: case 0x1EED: case 0x1EF1:
          output += "u"; break;
        case 0x1EF2: case 0x00DD: case 0x1EF8: case 0x1EF6: case 0x1EF4:
          output += "Y"; break;
        case 0x1EF3: case 0x00FD: case 0x1EF9: case 0x1EF7: case 0x1EF5:
          output += "y"; break;
        case 0x0110:
          output += "D"; break;
        case 0x0111:
          output += "d"; break;
        default:
          break;
      }

      i++;
      continue;
    }

    if (c >= 0xE0 && c <= 0xEF && i + 2 < input.length()) {
      unsigned char c2 = (unsigned char)input.charAt(i + 1);
      unsigned char c3 = (unsigned char)input.charAt(i + 2);

      if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) {
        continue;
      }

      uint32_t utf8Code = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);

      switch (utf8Code) {
        case 0x1EA0: case 0x1EA2: case 0x1EA4: case 0x1EA6: case 0x1EA8: case 0x1EAA: case 0x1EAC:
        case 0x1EAE: case 0x1EB0: case 0x1EB2: case 0x1EB4: case 0x1EB6:
          output += "A"; break;
        case 0x1EA1: case 0x1EA3: case 0x1EA5: case 0x1EA7: case 0x1EA9: case 0x1EAB: case 0x1EAD:
        case 0x1EAF: case 0x1EB1: case 0x1EB3: case 0x1EB5: case 0x1EB7:
          output += "a"; break;
        case 0x1EB8: case 0x1EBA: case 0x1EBC: case 0x1EBE: case 0x1EC0: case 0x1EC2: case 0x1EC4: case 0x1EC6:
          output += "E"; break;
        case 0x1EB9: case 0x1EBB: case 0x1EBD: case 0x1EBF: case 0x1EC1: case 0x1EC3: case 0x1EC5: case 0x1EC7:
          output += "e"; break;
        case 0x1EC8: case 0x1ECA:
          output += "I"; break;
        case 0x1EC9: case 0x1ECB:
          output += "i"; break;
        case 0x1ECC: case 0x1ECE: case 0x1ED0: case 0x1ED2: case 0x1ED4: case 0x1ED6: case 0x1ED8:
        case 0x1EDA: case 0x1EDC: case 0x1EDE: case 0x1EE0: case 0x1EE2:
          output += "O"; break;
        case 0x1ECD: case 0x1ECF: case 0x1ED1: case 0x1ED3: case 0x1ED5: case 0x1ED7: case 0x1ED9:
        case 0x1EDB: case 0x1EDD: case 0x1EDF: case 0x1EE1: case 0x1EE3:
          output += "o"; break;
        case 0x1EE4: case 0x1EE6: case 0x1EE8: case 0x1EEA: case 0x1EEC: case 0x1EEE: case 0x1EF0:
          output += "U"; break;
        case 0x1EE5: case 0x1EE7: case 0x1EE9: case 0x1EEB: case 0x1EED: case 0x1EEF: case 0x1EF1:
          output += "u"; break;
        case 0x1EF2: case 0x1EF4: case 0x1EF6: case 0x1EF8:
          output += "Y"; break;
        case 0x1EF3: case 0x1EF5: case 0x1EF7: case 0x1EF9:
          output += "y"; break;
        default:
          break;
      }

      i += 2;
      continue;
    }
  }

  return output;
}

// =====================================================
// BLE Callbacks
// =====================================================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    g_deviceConnected = true;
    g_lastActivityTime = millis();
    Serial.println("SmartHelmet app connected!");
  }

  void onDisconnect(BLEServer* pServer) override {
    g_deviceConnected = false;
    BLEDevice::startAdvertising();
    g_navInfo = NavigationInfo();
    Serial.println("SmartHelmet app disconnected!");
  }
};

class MyCharWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    g_lastActivityTime = millis();
    lastDataReceived = millis();
    totalDataReceived++;

    g_receivedData = String(pCharacteristic->getValue().c_str());

    Serial.println();
    Serial.println("=============== DATA RECEIVED ===============");
    Serial.printf("Packet #%lu | Size: %d bytes\n", totalDataReceived, g_receivedData.length());
    Serial.println("Raw JSON data:");
    Serial.println("----------------------------------------------");
    Serial.println(g_receivedData);
    Serial.println("----------------------------------------------");

    if (g_receivedData.startsWith("{") && g_receivedData.endsWith("}")) {
      g_isNaviDataUpdated = true;
      Serial.println("JSON format detected - OK");
    } else {
      Serial.println("Non-JSON data received");
    }

    Serial.println("============================================");
  }
};

// =====================================================
// JSON Parser - React Native BLE Service Format
// =====================================================
bool parseNavigationJSON(const String& jsonStr) {
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonStr);

  if (error) {
    failedParses++;
    Serial.printf("JSON Parse Error: %s\n", error.c_str());
    return false;
  }

  if (doc.containsKey("device")) {
    JsonObject device = doc["device"];
    if (device.containsKey("name")) {
      g_deviceName = device["name"].as<String>();
    }
    if (device.containsKey("id")) {
      g_deviceId = device["id"].as<String>();
    }
  }

  if (doc.containsKey("nav")) {
    int navValue = doc["nav"].as<int>();
    g_navInfo.isNavigating = (navValue == 1);
  } else {
    g_navInfo.isNavigating = false;
  }

  if (doc.containsKey("inst")) {
    g_navInfo.currentInstruction = doc["inst"].as<String>();
  } else {
    g_navInfo.currentInstruction = "";
  }

  if (doc.containsKey("next")) {
    g_navInfo.nextInstruction = doc["next"].as<String>();
  } else {
    g_navInfo.nextInstruction = "";
  }

  if (doc.containsKey("dist")) {
    g_navInfo.distanceToTurn = doc["dist"].as<int>();
  } else {
    g_navInfo.distanceToTurn = 0;
  }

  if (doc.containsKey("street")) {
    g_navInfo.currentStreet = doc["street"].as<String>();
  } else {
    g_navInfo.currentStreet = "";
  }

  if (doc.containsKey("nstreet")) {
    g_navInfo.nextStreet = doc["nstreet"].as<String>();
  } else {
    g_navInfo.nextStreet = "";
  }

  if (doc.containsKey("dir")) {
    int dirCode = doc["dir"].as<int>();
    g_navInfo.nextDirection = convertDirectionCode(dirCode);
  } else {
    g_navInfo.nextDirection = "";
  }

  if (doc.containsKey("step")) {
    g_navInfo.stepIndex = doc["step"].as<int>();
  }

  if (doc.containsKey("total")) {
    g_navInfo.totalSteps = doc["total"].as<int>();
  }

  successfulParses++;
  return true;
}

// =====================================================
// Direction Icon Mapping
// =====================================================
const unsigned char* getDirectionIcon(const String& direction) {
  String dir = direction;
  dir.toUpperCase();

  if (dir.indexOf("TRAI") >= 0 || dir.indexOf("LEFT") >= 0) {
    return TRAI;
  } else if (dir.indexOf("PHAI") >= 0 || dir.indexOf("RIGHT") >= 0) {
    return PHAI;
  } else if (dir.indexOf("THANG") >= 0 || dir.indexOf("STRAIGHT") >= 0 ||
             dir.indexOf("TIEP TUC") >= 0 || dir.indexOf("CONTINUE") >= 0) {
    return THANG;
  } else {
    return POINT;
  }
}

// =====================================================
// Display Functions
// =====================================================
void printText(int x, int y, const String &text, int textSize = 1) {
  String cleanText = convertVietnameseToDisplay(text);

  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(cleanText);
}

void drawNavigationDisplay() {
  display.clearDisplay();

  if (!g_navInfo.isNavigating) {
    printText(10, 15, "READY", 2);

    if (g_navInfo.currentInstruction.length() > 0) {
      String instruction = g_navInfo.currentInstruction;
      if (instruction.length() > 20) {
        instruction = instruction.substring(0, 17) + "...";
      }
      printText(5, 40, instruction, 1);
    } else {
      printText(8, 40, "Cho ket noi dieu huong", 1);
    }

    if (g_deviceName.length() > 0) {
      String deviceInfo = "Thiet bi: " + g_deviceName;
      if (deviceInfo.length() > 21) {
        deviceInfo = deviceInfo.substring(0, 18) + "...";
      }
      printText(5, 54, deviceInfo, 1);
    }

    display.display();
    return;
  }

  if (g_navInfo.currentStreet.length() > 0) {
    String street = g_navInfo.currentStreet;
    if (street.length() > 18) {
      street = street.substring(0, 15) + "...";
    }
    printText(2, 2, street, 1);
  }

  if (g_navInfo.nextDirection.length() > 0) {
    display.drawBitmap(95, 2, getDirectionIcon(g_navInfo.nextDirection),
                      LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);
  }

  if (g_navInfo.distanceToTurn > 0) {
    String distStr;
    if (g_navInfo.distanceToTurn < 1000) {
      distStr = String(g_navInfo.distanceToTurn) + "m";
    } else {
      float km = g_navInfo.distanceToTurn / 1000.0;
      distStr = String(km, 1) + "km";
    }

    printText(2, 16, distStr, 2);
  }

  if (g_navInfo.nextDirection.length() > 0 || g_navInfo.nextStreet.length() > 0) {
    String navInfo = "";

    if (g_navInfo.nextDirection.length() > 0) {
      String direction = g_navInfo.nextDirection;
      if (direction == "TRAI") navInfo += "Re trai";
      else if (direction == "PHAI") navInfo += "Re phai";
      else if (direction == "THANG") navInfo += "Di thang";
      else if (direction == "QUAY DAU") navInfo += "Quay dau";
      else navInfo += "Tiep tuc";
    }

    if (g_navInfo.nextStreet.length() > 0) {
      if (navInfo.length() > 0) navInfo += ": ";
      navInfo += g_navInfo.nextStreet;
    }

    if (navInfo.length() > 0) {
      if (navInfo.length() > 21) {
        int cutPos = 21;
        for (int i = 20; i > 10; i--) {
          if (navInfo.charAt(i) == ' ' || navInfo.charAt(i) == ':') {
            cutPos = i + 1;
            break;
          }
        }

        String line1 = navInfo.substring(0, cutPos);
        String line2 = navInfo.substring(cutPos);

        printText(2, 45, line1, 1);
        printText(2, 55, line2, 1);
      } else {
        printText(2, 50, navInfo, 1);
      }
    }
  }

  display.display();
}

void drawConnectionStatus() {
  display.clearDisplay();

  printText(15, 20, "Mat ket noi", 2);
  printText(5, 45, "Ket noi SmartHelmet App", 1);

  display.display();
}

// =====================================================
// Setup
// =====================================================
void initHUD() {
  Serial.println("============================================");
  Serial.println("       ESP32-S3 SMARTHELMET NAVIGATION HUD    ");
  Serial.println("============================================");

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }

  Serial.println("OLED Display initialized");

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  printText(5, 15, "SMART", 2);
  printText(5, 35, "HELMET", 2);
  display.display();
  delay(3000);

  Serial.println("Initializing BLE...");

  BLEDevice::init("ESP32 SmartHelmet HUD");
  g_pServer = BLEDevice::createServer();
  g_pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = g_pServer->createService(SERVICE_UUID);

  g_pCharIndicate = pService->createCharacteristic(
    CHAR_INDICATE_UUID,
    BLECharacteristic::PROPERTY_INDICATE
  );
  g_pCharIndicate->addDescriptor(new BLE2902());
  g_pCharIndicate->setValue("ESP32 SmartHelmet Ready");

  BLECharacteristic* pCharWrite = pService->createCharacteristic(
    CHAR_WRITE_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharWrite->setCallbacks(new MyCharWriteCallbacks());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising started");
  Serial.println("Device name: ESP32 SmartHelmet HUD");
  Serial.println("Service UUID: " + String(SERVICE_UUID));
  Serial.println("Write Char UUID: " + String(CHAR_WRITE_UUID));
  Serial.println("Indicate Char UUID: " + String(CHAR_INDICATE_UUID));
  Serial.println("Ready to receive navigation data!");
  Serial.println("============================================");

  display.clearDisplay();
  printText(15, 15, "CHO KET NOI", 2);
  printText(8, 40, "Dang cho ket noi...", 1);
  display.display();
}

// =====================================================
// Main HUD Task
// =====================================================
void taskHUD(void* pvParameters) {
  static unsigned long lastStatusUpdate = 0;

  while (true) {
    if (g_deviceConnected) {
      if (g_isNaviDataUpdated) {
        g_isNaviDataUpdated = false;

        if (parseNavigationJSON(g_receivedData)) {
          drawNavigationDisplay();
        }
      }

      if (millis() - lastStatusUpdate > 30000) {
        lastStatusUpdate = millis();
        Serial.printf("Status: Connected | Data: %lu | Success: %lu | Errors: %lu\n",
                      totalDataReceived, successfulParses, failedParses);
        if (g_deviceName.length() > 0) {
          Serial.printf("Connected to: %s\n", g_deviceName.c_str());
        }
      }

    } else {
      if (millis() - lastStatusUpdate > 3000) {
        lastStatusUpdate = millis();
        drawConnectionStatus();
        Serial.println("Waiting for SmartHelmet app connection...");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
