#include "SmsGps.h"
#include <HardwareSerial.h>

HardwareSerial gpsSerial(1);
HardwareSerial gsmSerial(2);

static String nmeaLat = "";
static String nmeaLon = "";
static String latHemi = "";
static String lonHemi = "";
static bool gpsFix = false;

static float nmeaToDecimal(const String& raw, const String& hemi) {
  if (raw.length() < 4) return 0.0f;
  float val = raw.toFloat();
  int deg = (int)(val / 100.0f);
  float minutes = val - deg * 100.0f;
  float dec = deg + minutes / 60.0f;
  if (hemi == "S" || hemi == "W") dec = -dec;
  return dec;
}

static void parseNmeaLine(String line) {
  if (!line.startsWith("$GPRMC") && !line.startsWith("$GNRMC")) return;

  String fields[12];
  int count = 0;
  int idx = 0;
  while ((idx = line.indexOf(',')) >= 0 && count < 12) {
    fields[count++] = line.substring(0, idx);
    line = line.substring(idx + 1);
  }
  if (count < 7) return;

  gpsFix = (fields[2] == "A");
  if (gpsFix) {
    nmeaLat = fields[3];
    latHemi = fields[4];
    nmeaLon = fields[5];
    lonHemi = fields[6];
  }
}

static bool waitForGpsFix(unsigned long timeoutMs) {
  unsigned long start = millis();
  String line = "";

  while (millis() - start < timeoutMs) {
    while (gpsSerial.available()) {
      char c = (char)gpsSerial.read();
      if (c == '\n') {
        parseNmeaLine(line);
        line = "";
        if (gpsFix) return true;
      } else if (c != '\r') {
        line += c;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return gpsFix;
}

static String readGsmResponse(unsigned long timeoutMs) {
  String resp = "";
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (gsmSerial.available()) resp += (char)gsmSerial.read();
    if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return resp;
}

static bool gsmCommand(const String& cmd, const String& expect,
                       unsigned long timeoutMs, int retries = 3) {
  for (int i = 0; i < retries; i++) {
    gsmSerial.println(cmd);
    String resp = readGsmResponse(timeoutMs);
    if (resp.indexOf(expect) >= 0) return true;
  }
  return false;
}

static bool sendSms(const String& message) {
  if (!gsmCommand("AT", "OK", 2000)) return false;
  if (!gsmCommand("AT+CMGF=1", "OK", 2000)) return false;

  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(SOS_PHONE);
  gsmSerial.println("\"");

  String resp = "";
  unsigned long start = millis();
  while (millis() - start < 5000 && resp.indexOf('>') < 0) {
    while (gsmSerial.available()) resp += (char)gsmSerial.read();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (resp.indexOf('>') < 0) return false;

  gsmSerial.print(message);
  gsmSerial.write(26);

  resp = readGsmResponse(30000);
  return resp.indexOf("+CMGS") >= 0 || resp.indexOf("OK") >= 0;
}

static String buildSosMessage() {
  String msg = "SOS! SmartHelmet: rider fall detected. ";
  if (gpsFix) {
    float lat = nmeaToDecimal(nmeaLat, latHemi);
    float lon = nmeaToDecimal(nmeaLon, lonHemi);
    msg += "Location: https://maps.google.com/?q=";
    msg += String(lat, 6);
    msg += ",";
    msg += String(lon, 6);
  } else {
    msg += "GPS location unavailable.";
  }
  return msg;
}

void initSmsGps() {
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  gsmSerial.begin(GSM_BAUD, SERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);
  vTaskDelay(pdMS_TO_TICKS(3000));
  Serial.println("GPS + GSM modules initialized");
}

void taskSMS(void*) {
  vTaskSuspend(NULL);

  for (;;) {
    Serial.println("[SMS] Preparing SOS message...");

    bool fixed = waitForGpsFix(GPS_FIX_TIMEOUT_MS);
    if (!fixed) Serial.println("[SMS] GPS fix timeout, sending without location");

    String msg = buildSosMessage();
    bool ok = sendSms(msg);
    Serial.println(ok ? "[SMS] SOS sent" : "[SMS] SOS FAILED");

    vTaskSuspend(NULL);
  }
}
