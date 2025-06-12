# Hướng dẫn Cài đặt - Nhà Thông minh ESP32 & Web
Dự án này xây dựng một hệ thống nhà thông minh đơn giản sử dụng ESP32 để thu thập dữ liệu từ cảm biến và điều khiển thiết bị. Dữ liệu được truyền qua giao thức MQTT và hiển thị trên một giao diện web thời gian thực được xây dựng bằng Node.js.

## Cấu Trúc Hệ Thống
1.  **Thiết bị (ESP32):** Đọc dữ liệu từ cảm biến (Nhiệt độ, Độ ẩm, Ánh sáng) và điều khiển Relay. Gửi và nhận dữ liệu qua MQTT.
2.  **Trung gian (MQTT Broker):** Sử dụng HiveMQ Cloud làm broker trung gian để nhận và chuyển tiếp tin nhắn.
3.  **Backend (Node.js):** Server chạy trên máy tính, kết nối với MQTT Broker để nhận dữ liệu từ ESP32 và gửi đến trình duyệt qua WebSocket. Nó cũng nhận lệnh điều khiển từ trình duyệt và gửi tới ESP32.
4.  **Frontend (Trình duyệt):** Giao diện web để người dùng theo dõi và điều khiển hệ thống.

## Yêu Cầu
### Phần cứng
* Bo mạch ESP32 Dev Kit.
* Cảm biến nhiệt độ, độ ẩm DHT11/DHT22.
* Cảm biến ánh sáng (Quang trở LDR).
* Module Relay 5V.
* Nút nhấn, điện trở, dây cắm và breadboard.

### Phần mềm
* **Arduino IDE** hoặc **VS Code với PlatformIO**.
* **Node.js và npm:** Tải và cài đặt từ [https://nodejs.org/](https://nodejs.org/) (chọn phiên bản LTS).

## Hướng Dẫn Cài Đặt
Thực hiện theo các bước sau để thiết lập và chạy toàn bộ hệ thống.

### **Bước 1: Cài đặt Phần cứng & Nạp Code ESP32**
1.  **Lắp ráp mạch:** Kết nối các cảm biến, relay và nút nhấn với bo mạch ESP32 theo sơ đồ mạch đã thiết kế.
2.  **Cài đặt thư viện Arduino:** Mở Arduino IDE, vào `Tools -> Manage Libraries...` và cài đặt các thư viện sau:
    * `PubSubClient` (của Nick O'Leary)
    * `Adafruit DHT sensor library`
    * Thư viện cho màn hình OLED (ví dụ: `Adafruit SSD1306` và `Adafruit GFX Library`)
3.  **Cấu hình Code ESP32:**
    * Mở file code `.ino` của dự án.
    * Tìm và thay đổi các thông tin sau cho phù hợp với mạng và tài khoản của bạn:
        ```cpp
        // Cấu hình WiFi
        const char* ssid = "TEN_WIFI_CUA_BAN";
        const char* password = "MAT_KHAU_WIFI";

        // Cấu hình MQTT Broker (HiveMQ Cloud)
        const char* mqtt_server = "xxxxxxxx.s1.eu.hivemq.cloud"; // Thay bằng Hostname của bạn
        const char* mqtt_user = "TEN_USER_HIVEMQ"; // Thay bằng Username của bạn
        const char* mqtt_password = "MAT_KHAU_HIVEMQ"; // Thay bằng Password của bạn
        ```
4.  **Nạp Code:**
    * Chọn đúng Board (`ESP32 Dev Module`) và Port (COMx) trong menu `Tools`.
    * Nhấn nút `Upload` để nạp code vào ESP32.
5.  **Kiểm tra:** Mở `Serial Monitor` và đặt tốc độ Baud là **115200**. Bạn sẽ thấy các log về việc kết nối WiFi và publish dữ liệu MQTT.

### **Bước 2: Cài đặt Backend (Node.js Server)**
1.  **Mở Terminal/CMD:** Mở một cửa sổ dòng lệnh và dùng lệnh `cd` để điều hướng đến thư mục `backend` của dự án.
    ```bash
    cd /duong/dan/den/project/Smart_Home_Web/backend
    ```
2.  **Cài đặt thư viện:** Chạy lệnh sau. Nó sẽ tự động cài tất cả các gói cần thiết được định nghĩa trong `package.json`.
    ```bash
    npm install
    ```
3.  **Cấu hình Backend:**
    * Mở file `server.js` trong thư mục `backend`.
    * Tìm và thay đổi các thông tin MQTT Broker cho khớp với thông tin bạn đã cấu hình trên ESP32.
        ```javascript
        const MQTT_BROKER_URL = 'mqtts://xxxxxxxx.s1.eu.hivemq.cloud'; // Thay bằng Hostname (thêm mqtts://)
        const MQTT_OPTIONS = {
            port: 8883,
            username: 'TEN_USER_HIVEMQ', // Thay bằng Username của bạn
            password: 'MAT_KHAU_HIVEMQ', // Thay bằng Password của bạn
            protocol: 'mqtts',
        };
        ```
    * Lưu file `server.js` lại.

### **Bước 3: Khởi Chạy Hệ Thống**
1.  **Cấp nguồn cho ESP32:** Đảm bảo bo mạch ESP32 đã được cắm nguồn và đang chạy.
2.  **Chạy Backend Server:** Trong cửa sổ dòng lệnh đang ở thư mục `backend`, chạy lệnh:
    ```bash
    node server.js
    ```
    Bạn sẽ thấy thông báo "Server đang chạy tại http://localhost:3000" và "Đã kết nối thành công đến MQTT Broker!".
3.  **Mở Giao diện Web:**
    * Mở trình duyệt web (Chrome, Firefox,...).
    * Truy cập vào địa chỉ: **`http://localhost:3000`**
    * Giao diện sẽ hiển thị và các thông số sẽ được cập nhật theo thời gian thực.

## Gỡ Lỗi Thường Gặp
* **Không thấy dữ liệu trên web:**
    1.  Kiểm tra `Serial Monitor` của ESP32 xem có log `MQTT Published` không.
    2.  Kiểm tra `WebSocket Client` trên HiveMQ Cloud xem có nhận được tin nhắn không.
    3.  Kiểm tra cửa sổ `cmd` chạy server xem có log `Nhận được trạng thái từ ESP32` không.
* **Lỗi `ECONNREFUSED` trên server:** Sai thông tin MQTT Broker trong file `server.js`.
* **Ký tự lạ trên Serial Monitor:** Sai tốc độ Baud. Hãy chọn `115200`.

Chúc bạn cài đặt thành công!