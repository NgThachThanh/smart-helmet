#include "VapCo.h"
#include <Wire.h>
#include <MPU6050.h>
#include <math.h>

#define CANCEL_PIN 4

extern TaskHandle_t smsTaskHandle;

struct SmartHelmetSensor::Impl {
  int sdaPin, sclPin, ledPin;
  MPU6050 mpu;

  float axg, ayg, azg, gxdps, gydps, gzdps;
  float aTot, gTot, roll, pitch;
  float aFilt = 0, rollFilt = 0;

  float baseThreshold = 2.5f;
  float speedFactor = 1.0f;
  float roadFactor = 1.2f;
  float vietnamFactor = 0.8f;
  float thresholdFinal = 2.4f;

  const unsigned long RECOVERY_WINDOW_MS = 3000;
  const float RECOVER_TILT_DEG = 30.0f;
  const float RECOVER_G_MIN = 0.8f;
  const float RECOVER_G_MAX = 1.2f;
  const unsigned long ALERT_DURATION_MS = 10000;

  enum Stage { NORMAL, RECOVERY, ALERT };
  Stage state = NORMAL;
  unsigned long stageStart = 0;
  unsigned long crashTime = 0;
  int lastPrintedRemain = -1;
  unsigned long lastBlink = 0;
  bool ledState = false;

  Impl(int sda, int scl, int led) : sdaPin(sda), sclPin(scl), ledPin(led) {}

  void begin() {
    mpu.initialize();

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
    pinMode(CANCEL_PIN, INPUT_PULLUP);

    if (!mpu.testConnection()) {
      Serial.println("MPU6050 not connected!");
      while (true) delay(1000);
    }

    mpu.setFullScaleAccelRange(MPU6050_IMU::MPU6050_ACCEL_FS_16);
    mpu.setFullScaleGyroRange(MPU6050_IMU::MPU6050_GYRO_FS_2000);
    mpu.setDLPFMode(MPU6050_IMU::MPU6050_DLPF_BW_42);
    Serial.println("MPU6050 ready");
  }

  void readSensor() {
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    axg = ax / 2048.0f; ayg = ay / 2048.0f; azg = az / 2048.0f;
    gxdps = gx / 16.4f; gydps = gy / 16.4f; gzdps = gz / 16.4f;

    aTot = sqrtf(axg * axg + ayg * ayg + azg * azg);
    gTot = sqrtf(gxdps * gxdps + gydps * gydps + gzdps * gzdps);
    roll = atan2f(ayg, azg) * 180.0f / PI;
    pitch = atan2f(-axg, sqrtf(ayg * ayg + azg * azg)) * 180.0f / PI;
  }

  float kalman(float x) {
    static float est = 1.0f, P = 1.0f, K;
    float Pp = P + 0.01f;
    K = Pp / (Pp + 0.1f);
    est += K * (x - est);
    P = (1 - K) * Pp;
    return est;
  }

  float complementary(float accelAng, float gyroRate, float dt) {
    static float alpha = 0.96f;
    return rollFilt = alpha * (rollFilt + gyroRate * dt) + (1 - alpha) * accelAng;
  }

  void resetLed() {
    digitalWrite(ledPin, LOW);
    ledState = false;
  }

  void handleAlert(unsigned long now) {
    if (digitalRead(CANCEL_PIN) == LOW) {
      state = NORMAL;
      resetLed();
      Serial.println("Alert cancelled, resuming monitoring...");
      return;
    }

    unsigned long elapsed = now - crashTime;

    int remain = (ALERT_DURATION_MS - elapsed) / 1000;
    if (remain != lastPrintedRemain && remain >= 0) {
      lastPrintedRemain = remain;
      Serial.printf("%ds left to cancel alert...\n", remain);
    }

    if (now - lastBlink >= 500) {
      lastBlink = now;
      ledState = !ledState;
      digitalWrite(ledPin, ledState ? HIGH : LOW);
    }

    if (elapsed >= ALERT_DURATION_MS) {
      resetLed();
      vTaskResume(smsTaskHandle);
      Serial.println("No response - SOS SMS dispatched");
      state = NORMAL;
    }
  }

  void detectCrash(unsigned long now) {
    thresholdFinal = baseThreshold * speedFactor * roadFactor * vietnamFactor;

    switch (state) {
      case NORMAL:
        if (aFilt > thresholdFinal || fabsf(rollFilt) > 60.0f) {
          state = RECOVERY;
          stageStart = now;
          Serial.println("Stage 1: potential crash, checking recovery...");
        }
        break;

      case RECOVERY:
        if (fabsf(rollFilt) < RECOVER_TILT_DEG && aFilt > RECOVER_G_MIN && aFilt < RECOVER_G_MAX) {
          state = NORMAL;
          Serial.println("Posture recovered - false positive filtered");
        } else if (now - stageStart > RECOVERY_WINDOW_MS) {
          state = ALERT;
          crashTime = now;
          lastPrintedRemain = -1;
          Serial.println("Stage 3: no recovery in 3s - ALERT countdown started");
        }
        break;

      case ALERT:
        handleAlert(now);
        break;
    }
  }

  void update() {
    static unsigned long prev = millis();
    unsigned long now = millis();
    float dt = (now - prev) / 1000.0f;
    prev = now;

    readSensor();
    aFilt = kalman(aTot);
    rollFilt = complementary(roll, gxdps, dt);

    detectCrash(now);
  }
};

SmartHelmetSensor::SmartHelmetSensor(int sda, int scl, int led)
  : impl(new Impl(sda, scl, led)) {}
SmartHelmetSensor::~SmartHelmetSensor() { delete impl; }
void SmartHelmetSensor::begin() { impl->begin(); }
void SmartHelmetSensor::update() { impl->update(); }
