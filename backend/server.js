// server.js

// --- 1. IMPORT CÁC THƯ VIỆN CẦN THIẾT ---
const express = require('express');
const http = require('http');
const path = require('path');
const mqtt = require('mqtt');
const { WebSocketServer } = require('ws');
const cors = require('cors');

// --- 2. KHỞI TẠO CÁC BIẾN VÀ CẤU HÌNH ---
const app = express();
const server = http.createServer(app);
const wss = new WebSocketServer({ server });

const PORT = 3000;
const MQTT_BROKER_URL = 'mqtts://11a47c24bc5f4dcea8db3370434ae364.s1.eu.hivemq.cloud'; // Ví dụ: 'mqtts://xxxxxxxx.s1.eu.hivemq.cloud'
const MQTT_OPTIONS = {
    port: 8883, // Port cho mqtts
    username: 'admin',
    password: 'Admin1234',
    protocol: 'mqtts', // Sử dụng 'mqtts' cho kết nối bảo mật
    clean: true,
};

// --- CÁC TOPIC MQTT ---
const STATUS_TOPIC = 'esp32/smarthome/status';
const RELAY_SET_TOPIC = 'esp32/smarthome/relay/set';
const MODE_SET_TOPIC = 'esp32/smarthome/mode/set';
const THRESHOLD_SET_TOPIC = 'esp32/smarthome/threshold/set';

// Biến lưu trữ trạng thái cuối cùng nhận được từ ESP32
let lastDeviceState = {
    temperature: null,
    humidity: null,
    light_percent: null,
    relay_status: "OFF",
    mode: "AUTO",
    threshold: 800
};

// --- 3. CẤU HÌNH EXPRESS VÀ MIDDLEWARE ---
app.use(cors()); // Cho phép cross-origin requests
app.use(express.json()); // Middleware để parse JSON body từ các request POST

// Phục vụ các file tĩnh từ thư mục cha (Smart_Home_Web)
app.use(express.static(path.join(__dirname, '..')));

// --- 4. KẾT NỐI VỚI MQTT BROKER ---
console.log('Đang kết nối đến MQTT Broker...');
const mqttClient = mqtt.connect(MQTT_BROKER_URL, MQTT_OPTIONS);

mqttClient.on('connect', () => {
    console.log('✅ Đã kết nối thành công đến MQTT Broker!');
    // Subscribe vào topic trạng thái của ESP32
    mqttClient.subscribe(STATUS_TOPIC, (err) => {
        if (!err) {
            console.log(`Đã subscribe vào topic: ${STATUS_TOPIC}`);
        }
    });
});

mqttClient.on('error', (err) => {
    console.error('Lỗi kết nối MQTT:', err);
});

// --- 5. XỬ LÝ DỮ LIỆU TỪ MQTT BROKER ---
mqttClient.on('message', (topic, message) => {
    if (topic === STATUS_TOPIC) {
        try {
            const status = JSON.parse(message.toString());
            console.log('Nhận được trạng thái từ ESP32:', status);
            
            // Cập nhật trạng thái cuối cùng
            lastDeviceState = { ...lastDeviceState, ...status };

            // Gửi trạng thái mới nhất đến tất cả các client đang kết nối qua WebSocket
            wss.clients.forEach(client => {
                if (client.readyState === client.OPEN) {
                    client.send(JSON.stringify(lastDeviceState));
                }
            });
        } catch (e) {
            console.error('Lỗi parse JSON từ MQTT:', e);
        }
    }
});

// --- 6. TẠO CÁC ĐƯỜNG DẪN API ĐỂ GIAO DIỆN WEB GỌI ---

// API để lấy trạng thái ban đầu khi tải trang
app.get('/api/smarthome/state', (req, res) => {
    res.json(lastDeviceState);
});

// API để điều khiển Relay (đèn)
app.post('/api/smarthome/relay/set', (req, res) => {
    const { command } = req.body; // Lấy command "ON" hoặc "OFF"
    if (command === 'ON' || command === 'OFF') {
        console.log(`Gửi lệnh đến ${RELAY_SET_TOPIC}: ${command}`);
        mqttClient.publish(RELAY_SET_TOPIC, command);
        res.json({ status: 'success', message: `Đã gửi lệnh ${command}` });
    } else {
        res.status(400).json({ status: 'error', message: 'Lệnh không hợp lệ' });
    }
});

// API để điều khiển Chế độ
app.post('/api/smarthome/mode/set', (req, res) => {
    const { mode } = req.body; // Lấy mode "AUTO" hoặc "MANUAL"
    if (mode === 'AUTO' || mode === 'MANUAL') {
        console.log(`Gửi lệnh đến ${MODE_SET_TOPIC}: ${mode}`);
        mqttClient.publish(MODE_SET_TOPIC, mode);
        res.json({ status: 'success', message: `Đã gửi lệnh ${mode}` });
    } else {
        res.status(400).json({ status: 'error', message: 'Chế độ không hợp lệ' });
    }
});

// API để đặt Ngưỡng sáng
app.post('/api/smarthome/threshold/set', (req, res) => {
    const { value } = req.body; // Lấy giá trị ngưỡng
    if (typeof value === 'number') {
        console.log(`Gửi lệnh đến ${THRESHOLD_SET_TOPIC}: ${value.toString()}`);
        mqttClient.publish(THRESHOLD_SET_TOPIC, value.toString());
        res.json({ status: 'success', message: `Đã gửi giá trị ngưỡng ${value}` });
    } else {
        res.status(400).json({ status: 'error', message: 'Giá trị không hợp lệ' });
    }
});

// --- 7. XỬ LÝ KẾT NỐI WEBSOCKET ---
wss.on('connection', ws => {
    console.log('✅ Một client đã kết nối qua WebSocket!');
    
    // Gửi ngay trạng thái hiện tại cho client vừa kết nối
    ws.send(JSON.stringify(lastDeviceState));

    ws.on('close', () => {
        console.log('Client đã ngắt kết nối WebSocket.');
    });
});


// --- 8. KHỞI ĐỘNG SERVER ---
server.listen(PORT, () => {
    console.log(`🚀 Server đang chạy tại http://localhost:${PORT}`);
});