#include <WiFi.h> // Thay đổi từ ESP8266WiFi.h
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <DHT.h>
#include <Wire.h>

// --- Cấu hình MQTT (Giữ nguyên từ code ESP8266 mẫu hoặc thay đổi theo bạn) ---
const char* ssid = "VIETTEL_C329D4";      // Tên Wifi của bạn
const char* password = "123456a@";   // Mật khẩu Wifi

// THAY ĐỔI XXXXXXXXXX thành URL cluster HiveMQ Cloud của bạn
const char* mqtt_server = "11a47c24bc5f4dcea8db3370434ae364.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "admin"; // User MQTT
const char* mqtt_password = "Admin1234"; // Password MQTT
// --- Kết thúc cấu hình MQTT ---

// --- Cấu hình thiết bị ---
// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT11 settings
#define DHTPIN 27     // Pin DHT trên ESP32
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Pin configuration
#define LDR_PIN     34  // Pin LDR trên ESP32 (ADC1_CH6)
#define RELAY_PIN   12  // Pin Relay trên ESP32
#define BUTTON_PIN  14  // Pin Nút nhấn trên ESP32

// --- Biến trạng thái ---
int threshold = 800;  // Ngưỡng ánh sáng (theo giá trị đã đảo chiều 0-4095)
bool lightOn = false;
bool manualMode = false;
unsigned long lastManualPressTime = 0;
unsigned long manualTimeout = 10000;  // 10 giây tự động về cảm biến
unsigned long lastMqttPublishTime = 0;
const long mqttPublishInterval = 5000; // Gửi dữ liệu MQTT mỗi 5 giây

// --- Đối tượng MQTT ---
WiFiClientSecure espClient;
PubSubClient client(espClient);

// --- Các hàm chức năng ---

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA); // Quan trọng cho ESP32
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String incomingMessage = "";
  for (unsigned int i = 0; i < length; i++) {
    incomingMessage += (char)payload[i];
  }
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(incomingMessage);

  // Xử lý lệnh từ MQTT
  // Ví dụ: esp32/smarthome/relay/set với payload "ON" hoặc "OFF"
  if (String(topic) == "esp32/smarthome/relay/set") {
    if (incomingMessage == "ON") {
      lightOn = true;
      digitalWrite(RELAY_PIN, lightOn); // LOW để BẬT nếu relay kích hoạt mức thấp, HIGH nếu kích hoạt mức cao. Giả sử relay của bạn được nối để HIGH là BẬT.
      manualMode = true; // Chuyển sang thủ công khi có lệnh
      lastManualPressTime = millis(); // Reset timeout thủ công
      Serial.println("MQTT: Relay ON");
    } else if (incomingMessage == "OFF") {
      lightOn = false;
      digitalWrite(RELAY_PIN, lightOn);
      manualMode = true;
      lastManualPressTime = millis();
      Serial.println("MQTT: Relay OFF");
    }
    // Có thể publish lại trạng thái sau khi thay đổi
    // publishStatus(); 
  }
  // Ví dụ: esp32/smarthome/mode/set với payload "AUTO" hoặc "MANUAL"
  else if (String(topic) == "esp32/smarthome/mode/set") {
    if (incomingMessage == "AUTO") {
      manualMode = false;
      Serial.println("MQTT: Mode set to AUTO");
    } else if (incomingMessage == "MANUAL") {
      manualMode = true;
      lastManualPressTime = millis(); // Reset timeout để không bị quay về auto ngay
      Serial.println("MQTT: Mode set to MANUAL");
    }
  }
  // Ví dụ: esp32/smarthome/threshold/set với payload là số (ngưỡng ánh sáng)
  else if (String(topic) == "esp32/smarthome/threshold/set") {
    int newThreshold = incomingMessage.toInt();
    if (newThreshold > 0 && newThreshold < 4096) { // Kiểm tra giá trị hợp lệ
        threshold = newThreshold;
        Serial.print("MQTT: Threshold set to ");
        Serial.println(threshold);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientID = "ESP32Client-";
    clientID += String(random(0xffff), HEX);
    if (client.connect(clientID.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Đăng ký (subscribe) các topic để nhận lệnh
      client.subscribe("esp32/smarthome/relay/set");
      client.subscribe("esp32/smarthome/mode/set");
      client.subscribe("esp32/smarthome/threshold/set");
      Serial.println("Subscribed to control topics.");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void publishStatus() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int rawLight = analogRead(LDR_PIN);
  int lightLevel = 4095 - rawLight; // Đảo ngược giá trị LDR
  int lightPercent = map(lightLevel, 0, 4095, 0, 100);

  // Kiểm tra giá trị đọc từ DHT
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    // Có thể không gửi nếu lỗi, hoặc gửi với giá trị báo lỗi
  }

  DynamicJsonDocument doc(256); // Kích thước đủ cho dữ liệu
  doc["temperature"] = isnan(t) ? "error" : String(t,1); // Gửi "error" nếu không đọc được, hoặc giá trị với 1 chữ số thập phân
  doc["humidity"] = isnan(h) ? "error" : String(h,1);
  doc["light_percent"] = lightPercent;
  doc["light_level_raw"] = lightLevel; // Gửi thêm giá trị thô nếu cần
  doc["relay_status"] = lightOn ? "ON" : "OFF";
  doc["mode"] = manualMode ? "MANUAL" : "AUTO";

  char mqtt_message[200]; // Buffer cho JSON
  serializeJson(doc, mqtt_message);

  if (client.publish("esp32/smarthome/status", mqtt_message, true)) { // Gửi với retained=true
    Serial.print("MQTT Published: ");
    Serial.println(mqtt_message);
  } else {
    Serial.println("MQTT Publish failed.");
  }
}


void setup() {
  Serial.begin(115200);
  Wire.begin(); // Khởi tạo I2C cho OLED (thường là GPIO 21 SDA, 22 SCL trên ESP32)

  pinMode(LDR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Đảm bảo relay tắt khi khởi động (LOW hay HIGH tùy loại relay)
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Địa chỉ I2C của OLED thường là 0x3C
    Serial.println(F("Không tìm thấy màn hình OLED"));
    // Không dừng hẳn, vẫn cho chạy MQTT
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Khoi dong...");
    display.display();
    delay(1000);
  }

  Serial.println("Smart Home System - ESP32 with MQTT");

  setup_wifi();
  
  // Với HiveMQ Cloud, nếu không dùng CA cert, setInsecure() là cần thiết
  // Để bảo mật hơn, bạn nên dùng setCACert() với CA của HiveMQ
  espClient.setInsecure(); // Bỏ qua kiểm tra chứng chỉ server (dễ dùng, kém an toàn hơn)
  // espClient.setCACert(root_ca_hivemq); // Nếu bạn có root CA của HiveMQ

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Quan trọng để PubSubClient xử lý

  // Đọc cảm biến ánh sáng và đảo chiều giá trị
  int rawLight = analogRead(LDR_PIN);
  int lightLevel = 4095 - rawLight; // ESP32 ADC 12-bit
  int lightPercent = map(lightLevel, 0, 4095, 0, 100);

  int buttonState = digitalRead(BUTTON_PIN);

  // Xử lý nút nhấn chuyển chế độ thủ công
  if (buttonState == LOW) { // Nút được nhấn (INPUT_PULLUP nên LOW là nhấn)
    delay(50); // Chống dội phím đơn giản
    if (digitalRead(BUTTON_PIN) == LOW) {
        manualMode = true;
        lightOn = !lightOn; // Đảo trạng thái đèn
        digitalWrite(RELAY_PIN, lightOn); // Điều khiển relay (HIGH là BẬT, LOW là TẮT, tùy thuộc vào relay của bạn)
        lastManualPressTime = millis();
        Serial.println(lightOn ? "THU CONG: Bat den" : "THU CONG: Tat den");
        while(digitalRead(BUTTON_PIN) == LOW); // Chờ thả nút
    }
  }

  // Tự động quay lại chế độ cảm biến sau timeout
  if (manualMode && (millis() - lastManualPressTime > manualTimeout)) {
    manualMode = false;
    Serial.println("Tu chuyen ve che do cam bien sau timeout");
  }

  // Điều khiển đèn theo cảm biến nếu ở chế độ tự động
  if (!manualMode) {
    if (lightLevel < threshold) { // Nếu tối hơn ngưỡng
      if (!lightOn) { // Chỉ bật nếu đang tắt
        lightOn = true;
        digitalWrite(RELAY_PIN, lightOn);
        Serial.println("TU DONG: Bat den");
      }
    } else { // Nếu sáng hơn ngưỡng
      if (lightOn) { // Chỉ tắt nếu đang bật
        lightOn = false;
        digitalWrite(RELAY_PIN, lightOn);
        Serial.println("TU DONG: Tat den");
      }
    }
  }

  // Đọc nhiệt độ và độ ẩm (có thể di chuyển vào hàm publishStatus nếu chỉ cần cho MQTT)
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // In ra Serial (để debug)
  // Serial.print("Anh sang (raw): "); Serial.print(rawLight);
  // Serial.print(" | Anh sang (inv): "); Serial.print(lightLevel);
  // Serial.print(" | Anh sang (%): "); Serial.print(lightPercent);
  // Serial.print(" % | Den: "); Serial.print(lightOn ? "BAT" : "TAT");
  // Serial.print(" | Che do: "); Serial.println(manualMode ? "THU CONG" : "TU DONG");
  // if (!isnan(t) && !isnan(h)) {
  //   Serial.print("Nhiet do: "); Serial.print(t);
  //   Serial.print(" *C | Do am: "); Serial.println(h);
  // } else {
  //   Serial.println("Loi doc DHT11!");
  // }


  // Hiển thị lên OLED
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Kiểm tra lại kết nối OLED
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Sang: "); display.print(lightPercent); display.println("%");
    display.print("Den: "); display.println(lightOn ? "BAT" : "TAT");
    display.print("Mode: "); display.println(manualMode ? "MANUAL" : "AUTO");

    display.print("Nhiet: ");
    if (isnan(t)) display.print("Loi"); else display.print(t, 1); // 1 chữ số thập phân
    display.println(" C");

    display.print("Am: ");
    if (isnan(h)) display.print("Loi"); else display.print(h, 1);
    display.println(" %");
    
    // Hiển thị trạng thái WiFi và MQTT
    display.setCursor(0, SCREEN_HEIGHT - 8); // Dòng cuối
    if(WiFi.status() == WL_CONNECTED) {
        display.print("WiFi:OK ");
        if(client.connected()){
            display.print("MQTT:OK");
        } else {
            display.print("MQTT:ERR");
        }
    } else {
        display.print("WiFi:ERR");
    }
    display.display();
  }


  // Gửi dữ liệu qua MQTT theo định kỳ
  if (millis() - lastMqttPublishTime > mqttPublishInterval) {
    publishStatus();
    lastMqttPublishTime = millis();
  }

  delay(100); // Giảm delay để loop chạy nhanh hơn, xử lý MQTT tốt hơn
}