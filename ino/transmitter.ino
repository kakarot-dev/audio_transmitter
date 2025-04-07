#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <FS.h>
#include <LittleFS.h>
#include <HTTPClient.h>

#define CE_PIN  4
#define CSN_PIN 5
#define BUFFER_SIZE 32

RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "audio";
bool isTransmitting = false;
bool isConverting = false;
File audioFile;
int packetCount = 0;

const char* ssid = "joel";
const char* password = "00000000";
WebServer server(80);

// Markers for transmission via nRF24L01
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

void scanNetworks() {
  Serial.println("🔍 Scanning for networks...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    Serial.printf("  %d: %s (%d dBm) %s\n",
      i + 1,
      WiFi.SSID(i).c_str(),
      WiFi.RSSI(i),
      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Encrypted");
  }
}

void connectToWiFi() {
  Serial.printf("🔌 Connecting to %s...\n", ssid);
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n❌ Failed to connect to Wi-Fi.");
  }
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

void startTransmission() {
  audioFile = LittleFS.open("/audio.raw", "r");
  if (!audioFile) {
    Serial.println("❌ Failed to open audio.raw for transmission!");
    return;
  }
  packetCount = 0;
  isTransmitting = true;
  Serial.println("📤 Starting transmission...");
}

void convertToRaw() {
  Serial.println("☁️ Uploading MP3 and downloading converted RAW...");
  isConverting = true;
  File f = LittleFS.open("/recorded.mp3", "r");
  if (!f) {
    Serial.println("❌ Could not open recorded.mp3 for upload.");
    return;
  }

  WiFiClient client;
  if (!client.connect("192.168.59.172", 3000)) {
    Serial.println("❌ Connection to server failed!");
    f.close();
    return;
  }

  String boundary = "----ESP32Boundary";
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"audio\"; filename=\"recorded.mp3\"\r\n"
                "Content-Type: audio/mpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  int contentLength = head.length() + f.size() + tail.length();

  client.printf("POST /convert HTTP/1.1\r\n");
  client.printf("Host: 192.168.59.172:3000\r\n");
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %d\r\n", contentLength);
  client.printf("Connection: close\r\n\r\n");
  client.print(head);

  uint8_t buf[BUFFER_SIZE];
  while (f.available()) {
    int len = f.read(buf, BUFFER_SIZE);
    client.write(buf, len);
  }
  f.close();
  client.print(tail);

  // Skip headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }
  
  File out = LittleFS.open("/audio.raw", "w");
  if (!out) {
    Serial.println("❌ Failed to open /audio.raw for writing");
    return;
  }

  while (client.connected() || client.available()) {
    int len = client.read(buf, BUFFER_SIZE);
    if (len > 0) out.write(buf, len);
  }
  out.close();
  client.stop();
  isConverting = false;
  Serial.println("🎉 Conversion done and saved as /audio.raw");

  startTransmission(); // 📤 Now start transmitting!
}

void handleFileRequest() {
  String path = server.uri();
  if (path == "/") path = "/index.html";
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    String contentType = "text/plain";
    if (path.endsWith(".html")) contentType = "text/html";
    else if (path.endsWith(".css")) contentType = "text/css";
    else if (path.endsWith(".js")) contentType = "application/javascript";
    else if (path.endsWith(".svg")) contentType = "image/svg+xml";
    else if (path.endsWith(".png")) contentType = "image/png";
    else if (path.endsWith(".ico")) contentType = "image/x-icon";
    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "404 Not Found");
  }
}

void setupWebServer() {
  server.onNotFound(handleFileRequest);

  server.on("/status", HTTP_GET, []() {
    String status = isTransmitting ? "transmitting" : isConverting ? "converting" : "idle";
    server.send(200, "text/plain", status);
  });

  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "OK");
  }, []() {
    static File f;
    static bool uploadFailed = false;
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
      Serial.println("📥 Upload started");
      LittleFS.remove("/recorded.mp3");
      delay(1000);
      f = LittleFS.open("/recorded.mp3", "w");
      uploadFailed = !f;
      if (uploadFailed) {
        Serial.println("❌ Failed to open recorded.mp3 for writing");
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (!uploadFailed && f) {
        f.write(upload.buf, upload.currentSize);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (f) f.close();
      Serial.println("✅ Upload complete");
      if (!uploadFailed) {
        convertToRaw(); // ✅ Only call if write was successful
      } else {
        Serial.println("❌ Upload failed, not converting.");
      }
    }
  });

  server.begin();
  Serial.println("🌐 Web server started.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  scanNetworks();
  connectToWiFi();

  LittleFS.begin();
  listFiles();
  setupWebServer();

  Serial.printf("Total space: %u bytes\n", LittleFS.totalBytes());
  Serial.printf("Used space: %u bytes\n", LittleFS.usedBytes());

  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_2MBPS);
  radio.setAutoAck(false);
  radio.openWritingPipe(address);
  radio.stopListening();
}

void loop() {
  server.handleClient();
  static uint8_t buffer[BUFFER_SIZE];

  if (isTransmitting && audioFile) {
    if (packetCount == 0) {
      Serial.println("📤 Sending start marker...");
      radio.write(START_OF_FILE_MARKER, BUFFER_SIZE);
      delay(100);
    }

    if (audioFile.available()) {
      size_t bytesRead = audioFile.read(buffer, BUFFER_SIZE);
      if (bytesRead == BUFFER_SIZE) {
        radio.write(buffer, BUFFER_SIZE);
        packetCount++;
        delayMicroseconds(1500);
      }
    } else {
      Serial.printf("✅ Transmission complete. Total packets: %d\n", packetCount);
      for (int i = 0; i < 10; i++) {
        radio.write(END_OF_FILE_MARKER, BUFFER_SIZE);
        delay(5);
      }
      audioFile.close();
      isTransmitting = false;
    }
  }
}
