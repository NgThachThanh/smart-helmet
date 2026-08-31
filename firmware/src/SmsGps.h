#ifndef SMSGPS_H
#define SMSGPS_H

#define GPS_RX_PIN 18
#define GPS_TX_PIN 17
#define GPS_BAUD 9600

#define GSM_RX_PIN 26
#define GSM_TX_PIN 27
#define GSM_BAUD 115200

#define SOS_PHONE "+84XXXXXXXXX"
#define GPS_FIX_TIMEOUT_MS 60000

void initSmsGps();
void taskSMS(void* pvParameters);

#endif
