#include "config.hpp"
#include "manchester.hpp"
#include "datalink.hpp"

static const uint8_t testData[] = {
    0x55,                            // Preamble
    0x7E,                            // Start
    0x02, 0x01, 0x05, 0x00,          // Header: DATA, seq=1, len=5, vol=0
    'H', 'e', 'l', 'l', 'o',         // Payload
    0x41, 0xFB,                      // CRC16
    0x7E                             // End
};
static const size_t testDataLen = sizeof(testData);

static datalink::Sender sender;
static datalink::Receiver receiver;
// static manchester::Sender manchSender;
// static manchester::Receiver manchReceiver;
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

    sender.begin(D1_TX_PIN, HALF_BIT_US);
    receiver.begin(D1_RX_PIN, HALF_BIT_US);

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

}

void txTask(void* pvParameters) {
  Serial.println("Started txTask");

  const char* message = "Hello";

  while (1) {
    Serial.println("Sending START...");
    sender.sendPreamble();
    sender.sendStart(3);
    vTaskDelay(1000);

    Serial.println("Sending DATA...");
    sender.sendPreamble();
    sender.sendDataFrame(reinterpret_cast<const uint8_t*>(message), strlen(message), 1);
    vTaskDelay(1000);

    Serial.println("Sending END...");
    sender.sendPreamble();
    sender.sendEnd();
    vTaskDelay(1000);

    Serial.println("Sending NACK...");
    sender.sendPreamble();
    sender.sendNack(1);
    vTaskDelay(1000);
  }
}

void rxTask(void* pvParameters) {
    while (true) {
      vTaskDelay(1);

      const Frame* frame = receiver.getFrame();
      if (!frame) continue;

      switch (frame->header.type) {
        case FrameType::START: {
          Serial.println("START");
          break;
        }

        case FrameType::DATA: {
          Serial.printf(
            "DATA seq=%u len=%u\n",
            frame->header.seq,
            frame->header.len
          );

          for (int i = 0; i < frame->header.len; ++i) {
            Serial.write(frame->payload[i]);
          }

          Serial.println();
          break;
        }

        case FrameType::END: {
          Serial.println("END");
          break;
        }

        case FrameType::NACK: {
          Serial.println("NACK");
          break;
        }
      }
    }
}

void onEdgeD1() {
  bool level = gpio_get_level((gpio_num_t)D1_RX_PIN);
  receiver.onEdge(level);
}

void onEdgeD2() {
  // TODO
}
