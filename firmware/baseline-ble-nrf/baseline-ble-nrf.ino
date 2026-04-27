#include <bluefruit.h>
#include "nrf_power.h"
#include "nrf_soc.h"

void connectCallback(uint16_t connHandle) {
  BLEConnection *connection = Bluefruit.Connection(connHandle);
  if (connection) {
    connection->requestConnectionParameter(80, 20, 600);
  }
}

void setup() {
  Bluefruit.autoConnLed(false);
  Bluefruit.configPrphBandwidth(BANDWIDTH_LOW);
  Bluefruit.begin();
  sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
  Bluefruit.Periph.setConnIntervalMS(100, 100);
  Bluefruit.Periph.setConnSlaveLatency(20);
  Bluefruit.Periph.setConnSupervisionTimeoutMS(6000);
  Bluefruit.setTxPower(-12);
  Bluefruit.setName("Baseline BLE");

  Bluefruit.Periph.setConnectCallback(connectCallback);

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(160, 1600);
  Bluefruit.Advertising.setFastTimeout(15);
  Bluefruit.Advertising.start(0);
}

void loop() {
  waitForEvent();
}
