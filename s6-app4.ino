#include "config.hpp"
#include "manchester.hpp"

static const uint8_t testData[] = {
    0x55,                            // Preamble
    0x7E,                            // Start
    0x02, 0x01, 0x05, 0x00,          // Header: DATA, seq=1, len=5, vol=0
    'H', 'e', 'l', 'l', 'o',         // Payload
    0x41, 0xFB,                      // CRC16
    0x7E                             // End
};
static const size_t testDataLen = sizeof(testData);

static manchester::Sender manchSender;
static manchester::Receiver manchReceiver;
// static uint8_t rxBuf[RX_BUF_SIZE];

static volatile int txCount = 0;
static volatile int rxCount = 0;
static volatile int passCount = 0;
static volatile int failCount = 0;

void txTask(void* pvParameters);
void rxTask(void* pvParameters);
void onEdgeD1();
void onEdgeD2();

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== Manchester Test ===");
    // Serial.printf("Device: %s, TX_GPIO=%d, RX_GPIO=%d\n", "DEVICE_1", TX_GPIO, RX_GPIO);
    // Serial.printf("Bit rate: %d bps, Half-bit: %d µs\n", 1000000 / BIT_PERIOD_US, HALF_BIT_US);
    // Serial.printf("Test data: %d bytes\n\n", testDataLen);

    manchSender.begin(D1_TX_PIN, HALF_BIT_US);
    manchReceiver.begin(D1_RX_PIN, HALF_BIT_US);

    pinMode(D1_RX_PIN, INPUT);
    pinMode(D1_TX_PIN, OUTPUT);
    pinMode(D2_RX_PIN, INPUT);
    pinMode(D2_TX_PIN, OUTPUT);

    // Initialize to low so when we start with preamble, first falling edge is always a bit transition and not an inbetween transition
    digitalWrite(D1_TX_PIN, LOW);
    digitalWrite(D2_TX_PIN, LOW);

    xTaskCreatePinnedToCore(rxTask, "rxTask", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(txTask, "txTask", 2048, NULL, 1, NULL, 1);

    attachInterrupt(digitalPinToInterrupt(D1_RX_PIN), onEdgeD1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(D2_RX_PIN), onEdgeD2, CHANGE);
}

void loop() { 
    // vTaskDelay(5000 / portTICK_PERIOD_MS);
    // Serial.printf("Stats - TX: %d, RX: %d, PASS: %d, FAIL: %d\n", txCount, rxCount, passCount, failCount);
}

void txTask(void* pvParameters) {
  Serial.println("Started txTask");

  while (1) {
    Serial.println("Sending buffer...");
    Serial.print("Data length: ");
    Serial.println(testDataLen);
    manchSender.sendBuffer(testData, testDataLen);
    vTaskDelay(1000);
    digitalWrite(D1_TX_PIN, LOW);
    vTaskDelay(1000);
    manchReceiver.unsync();
    vTaskDelay(2000);
  }
}

void rxTask(void* pvParameters) {
  Serial.println("Started rxTask");
  uint8_t byte;

  while (1) {
    if (manchReceiver.getByte(byte)) {
      Serial.print("Received byte: ");
      Serial.println(byte, HEX);
    }
    vTaskDelay(1);
  }
}

void onEdgeD1() {
  bool level = gpio_get_level((gpio_num_t)D1_RX_PIN);
  manchReceiver.onEdge(level);
}

void onEdgeD2() {
  // TODO
}
