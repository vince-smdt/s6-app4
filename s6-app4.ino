#include "config.hpp"
#include "protocol.hpp"

#ifdef DEVICE_1
static const char* donnees_txt[] = {
    "Ligne 1 - 2026-06-17 10:00:01 | Temp: 22.4 C | Humidite: 45.2 % | Node: 01",
    "Ligne 2 - 2026-06-17 10:05:01 | Temp: 22.5 C | Humidite: 45.1 % | Node: 01",
    "Ligne 3 - 2026-06-17 10:10:01 | Temp: 22.8 C | Humidite: 44.9 % | Node: 01",
    "Ligne 4 - 2026-06-17 10:15:01 | Temp: 23.1 C | Humidite: 44.8 % | Node: 01",
    "Ligne 5 - 2026-06-17 10:20:01 | Temp: 23.0 C | Humidite: 45.0 % | Node: 01",
};
static const int total_paquets = sizeof(donnees_txt) / sizeof(donnees_txt[0]);

static protocol::Sender sender;
static volatile unsigned long txStartUs, txEndUs;
static volatile bool txDone = false;
static volatile size_t totalBytes = 0;

void txTask(void* pvParameters);
void onEdgeD1();
#endif // DEVICE_1

#ifdef DEVICE_2
static protocol::Receiver receiver;
static uint8_t rxBuffer[512];
static volatile unsigned long rxStartUs, rxEndUs;
static volatile bool rxDone = false;

void rxTask(void* pvParameters);
void onEdgeD2();
#endif // DEVICE_2

static volatile bool startBenchmark = true;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== Protocol Benchmark ===");

#ifdef DEVICE_1
    sender.begin(D1_TX_PIN, D1_RX_PIN, HALF_BIT_US);

    pinMode(D1_RX_PIN, INPUT);
    pinMode(D1_TX_PIN, OUTPUT);
    digitalWrite(D1_TX_PIN, LOW);

    xTaskCreatePinnedToCore(txTask, "txTask", 4096, NULL, 1, NULL, 1);
    attachInterrupt(digitalPinToInterrupt(D1_RX_PIN), onEdgeD1, CHANGE);
#endif

#ifdef DEVICE_2
    receiver.begin(D2_TX_PIN, D2_RX_PIN, HALF_BIT_US);
    receiver.setBuffer(rxBuffer, sizeof(rxBuffer));

    pinMode(D2_RX_PIN, INPUT);
    pinMode(D2_TX_PIN, OUTPUT);
    digitalWrite(D2_TX_PIN, LOW);

    xTaskCreatePinnedToCore(rxTask, "rxTask", 4096, NULL, 1, NULL, 0);
    attachInterrupt(digitalPinToInterrupt(D2_RX_PIN), onEdgeD2, CHANGE);
#endif
}

void loop() {
#ifdef DEVICE_1
    if (txDone) {
        startBenchmark = false;
        txDone = false;

        unsigned long elapsed = txEndUs - txStartUs;
        float sec = elapsed / 1e6f;

        Serial.printf("\n--- Sender Results ---\n");
        Serial.printf("Bytes: %u  Time: %lu us (%.3f s)  ->  %.2f B/s\n",
                      totalBytes, elapsed, sec, totalBytes / sec);

        vTaskDelay(5000);
        startBenchmark = true;
    }
#endif

#ifdef DEVICE_2
    if (rxDone) {
        startBenchmark = false;
        rxDone = false;

        size_t received = receiver.available();
        unsigned long elapsed = rxEndUs - rxStartUs;
        float sec = elapsed / 1e6f;

        Serial.printf("\n--- Receiver Results ---\n");
        Serial.printf("Bytes: %u  Time: %lu us (%.3f s)  ->  %.2f B/s\n",
                      received, elapsed, sec, received / sec);

        vTaskDelay(5000);
        startBenchmark = true;
    }
#endif
}

#ifdef DEVICE_1
void txTask(void* pvParameters) {
    char data[512];

    while (true) {
        while (!startBenchmark) vTaskDelay(10);
        vTaskDelay(100);

        size_t offset = 0;
        for (int i = 0; i < total_paquets; i++) {
            size_t len = strlen(donnees_txt[i]);
            memcpy(data + offset, donnees_txt[i], len);
            offset += len;
        }
        totalBytes = offset;

        Serial.printf("Sending %u bytes in %d packets...\n", offset, total_paquets);

        txStartUs = micros();
#ifdef INJECT_ERROR
        sender.enableErrorInjection(3);
#endif
        sender.sendSession((const uint8_t*)data, offset);
        txEndUs = micros();

        txDone = true;
    }
}

void onEdgeD1() {
    bool level = gpio_get_level((gpio_num_t)D1_RX_PIN);
    sender.onEdge(level);
}
#endif // DEVICE_1

#ifdef DEVICE_2
void rxTask(void* pvParameters) {
    while (true) {
        while (!startBenchmark) vTaskDelay(10);
        vTaskDelay(50);
        receiver.reset();
        rxStartUs = micros();

        while (true) {
            auto status = receiver.poll();
            if (status == protocol::Receiver::Status::COMPLETE) {
                rxEndUs = micros();
                rxDone = true;
                break;
            }
            vTaskDelay(1);
        }
    }
}

void onEdgeD2() {
    bool level = gpio_get_level((gpio_num_t)D2_RX_PIN);
    receiver.onEdge(level);
}
#endif // DEVICE_2
