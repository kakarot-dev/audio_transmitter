#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <FS.h>
#include <LittleFS.h>
#include <driver/i2s.h>

#define CE_PIN  4
#define CSN_PIN 5
#define BUFFER_SIZE 32

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "audio";

File writingFile;
int packetCounter = 0;
bool recordingStarted = false;
uint8_t lastPacket[BUFFER_SIZE];

const uint8_t START_OF_FILE_MARKER[BUFFER_SIZE] = {
    0xAA, 0xBB, 0xCC, 0xDD, 0xAA, 0xBB, 0xCC, 0xDD,
    0xAA, 0xBB, 0xCC, 0xDD, 0xAA, 0xBB, 0xCC, 0xDD,
    0xAA, 0xBB, 0xCC, 0xDD, 0xAA, 0xBB, 0xCC, 0xDD,
    0xAA, 0xBB, 0xCC, 0xDD, 0xAA, 0xBB, 0xCC, 0xDD
};

const uint8_t END_OF_FILE_MARKER[BUFFER_SIZE] = {
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF,
    0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF
};

bool isStartOfFile(const uint8_t* data) {
    return memcmp(data, START_OF_FILE_MARKER, BUFFER_SIZE) == 0;
}

bool isEndOfFile(const uint8_t* data) {
    return memcmp(data, END_OF_FILE_MARKER, BUFFER_SIZE) == 0;
}

// I2S config
void setupI2S() {
    i2s_config_t config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = 4,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 22,
        .data_in_num = -1
    };
    i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

// Background task for audio playback (5 times)
void playAudioTask(void* param) {
    File audioFile = LittleFS.open("/input.raw", "r");
    if (!audioFile) {
        Serial.println("❌ Failed to open file for playback.");
        vTaskDelete(NULL);
    }

    setupI2S();
    uint8_t buffer[BUFFER_SIZE];

    Serial.println("🔁 Starting 5-time playback...");

    for (int round = 0; round < 5; round++) {
        Serial.printf("▶️ Round %d...\n", round + 1);
        audioFile.seek(0);
        while (audioFile.available()) {
            size_t bytesRead = audioFile.read(buffer, BUFFER_SIZE);
            size_t bytesWritten;
            i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
        }
        delay(100); // small pause between rounds
    }

    audioFile.close();
    i2s_driver_uninstall(I2S_NUM_0);
    Serial.println("✅ Playback finished.");
    vTaskDelete(NULL);
}

void listFiles(const char * dirname = "/") {
    Serial.printf("📁 Listing files in: %s\n", dirname);

    File root = LittleFS.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Failed to open directory.");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        Serial.printf("📄 %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    if (!LittleFS.begin()) {
        Serial.println("❌ Failed to mount LittleFS");
        return;
    }

    listFiles();

    radio.begin();
    radio.openReadingPipe(1, address);
    radio.setPALevel(RF24_PA_HIGH);
    radio.setDataRate(RF24_2MBPS);
    radio.setAutoAck(false);
    radio.startListening();
}

void loop() {
    static uint8_t buffer[BUFFER_SIZE];

    while (radio.available()) {
        radio.read(buffer, BUFFER_SIZE);

        if (!recordingStarted && isStartOfFile(buffer)) {
            LittleFS.remove("/input.raw");
            delay(1000);
            writingFile = LittleFS.open("/input.raw", "w");
            recordingStarted = true;
            Serial.println("🎙️ Start of file received, writing...");
            continue;
        }

        if (recordingStarted) {
            if (isEndOfFile(buffer)) {
                writingFile.close();
                recordingStarted = false;
                Serial.println("✅ Received EOF. File closed.");
                delay(200);

                // 🎵 Start 5-time playback task
                xTaskCreate(playAudioTask, "PlayAudioTask", 4096, NULL, 1, NULL);
                return;
            }

            writingFile.write(buffer, BUFFER_SIZE);
            memcpy(lastPacket, buffer, BUFFER_SIZE);
            packetCounter++;
        }
    }

    static unsigned long lastLog = 0;
    if (millis() - lastLog > 1000) {
        Serial.printf("📈 Packet count: %d\n📦 Last packet: ", packetCounter);
        for (int i = 0; i < BUFFER_SIZE; i++) {
            Serial.printf("%02X ", lastPacket[i]);
        }
        Serial.println();
        lastLog = millis();
    }
}
