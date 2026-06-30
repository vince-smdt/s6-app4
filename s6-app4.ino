#include "frame.hpp"
#include "crc16.hpp"
#include "manchester.hpp"

#define TX_PIN  4
#define RX_PIN  2

void IRAM_ATTR rxIsr(void* arg) {
  manchester::onEdge();
}

void txTask(void* pvParameters) {
  uint8_t data = 0;
  while (true) {
    Serial.print("TX: 0x");
    Serial.println(data, HEX);
    manchester::sendByte(data);
    vTaskDelay(1);
    data++;
  }
}

void rxTask(void* pvParameters) {
  while (true) {
    while (manchester::available()) {
      Serial.print("RX: 0x");
      Serial.println(manchester::read(), HEX);
    }
    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);

  manchester::beginTX(TX_PIN, 100000);
  manchester::beginRX(RX_PIN, 100000);

  gpio_config_t io = {};
  io.intr_type     = GPIO_INTR_ANYEDGE;
  io.pin_bit_mask  = 1ULL << RX_PIN;
  io.mode          = GPIO_MODE_INPUT;
  io.pull_up_en    = GPIO_PULLUP_DISABLE;
  io.pull_down_en  = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io);
  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)RX_PIN, rxIsr, nullptr);

  xTaskCreatePinnedToCore(txTask, "tx", 2048, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(rxTask, "rx", 2048, nullptr, 1, nullptr, 1);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
