#include "frame.hpp"
#include "crc16.hpp"
#include "manchester.hpp"
#include "rxLogic.hpp"

#define RECEIVER_RX_PIN     2
#define RECEIVER_TX_PIN     4
#define TRANSMITTER_RX_PIN  12
#define TRANSMITTER_TX_PIN  14

manchester::Transceiver manchRX, manchTX;

void IRAM_ATTR rxIsr(void* arg) {
  static_cast<manchester::Transceiver*>(arg)->onEdge();
}

void txTask(void* pvParameters) {
  uint8_t data = 0;
  while (true) {
    Serial.print("TX: 0x");
    Serial.println(PREAMBLE, HEX);
    manchTX.sendByte(PREAMBLE);
    vTaskDelay(10);

    Serial.print("TX: 0x");
    Serial.println(data, HEX);
    manchTX.sendByte(data);
    vTaskDelay(10);
    data++;

    Serial.print("TX: 0x");
    Serial.println(data, HEX);
    manchTX.sendByte(data);
    vTaskDelay(10);
    data++;
  }
}

void rxTask(void* pvParameters) {
  while (true) {
    String msg = rxLogic::read();
    if(msg != ""){
      Serial.println(msg);
    }
    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);

  manchRX.beginTX(RECEIVER_TX_PIN, 100000);
  manchRX.beginRX(RECEIVER_RX_PIN, 100000);
  manchTX.beginTX(TRANSMITTER_TX_PIN, 100000);
  manchTX.beginRX(TRANSMITTER_RX_PIN, 100000);

  gpio_config_t io = {};
  io.intr_type     = GPIO_INTR_ANYEDGE;
  io.pin_bit_mask  = 1ULL << RECEIVER_RX_PIN;
  io.mode          = GPIO_MODE_INPUT;
  io.pull_up_en    = GPIO_PULLUP_DISABLE;
  io.pull_down_en  = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io);

  io.pin_bit_mask  = 1ULL << TRANSMITTER_RX_PIN;
  gpio_config(&io);

  gpio_install_isr_service(0);
  gpio_isr_handler_add((gpio_num_t)RECEIVER_RX_PIN, rxIsr, &manchRX);
  gpio_isr_handler_add((gpio_num_t)TRANSMITTER_RX_PIN, rxIsr, &manchTX);

  xTaskCreatePinnedToCore(txTask, "tx", 2048, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(rxTask, "rx", 2048, nullptr, 1, nullptr, 1);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
