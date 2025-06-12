// script-control.js
document.addEventListener('DOMContentLoaded', () => {
    // --- Lấy tham chiếu đến các Element trên UI ---
    const temperatureValueEl = document.getElementById('temperature-value');
    const humidityValueEl = document.getElementById('humidity-value');
    const lightPercentValueEl = document.getElementById('light-percent-value');
    const mqttStatusIndicatorEl = document.getElementById('mqtt-status-indicator');
    const mqttStatusTextEl = document.getElementById('mqtt-status-text');

    const relayToggleSwitchEl = document.getElementById('relay-toggle-switch');
    const relayStatusTextEl = document.getElementById('relay-status-text');

    const modeAutoBtnEl = document.getElementById('mode-auto-btn');
    const modeManualBtnEl = document.getElementById('mode-manual-btn');
    const currentModeTextEl = document.getElementById('current-mode-text');

    const thresholdSliderEl = document.getElementById('threshold-slider');
    const thresholdValueDisplayEl = document.getElementById('threshold-value-display');
    const setThresholdBtnEl = document.getElementById('set-threshold-btn');

    // --- Biến lưu trữ trạng thái cục bộ ---
    let currentDeviceState = {};

    // --- Hàm Cập nhật Giao diện Người dùng (UI) ---
    function updateUI(state) {
        if (!state) return;
        currentDeviceState = state; // Cập nhật state cục bộ

        // Cập nhật hiển thị cảm biến
        temperatureValueEl.textContent = state.temperature !== null && !isNaN(state.temperature) ? `${parseFloat(state.temperature).toFixed(1)} °C` : '-- °C';
        humidityValueEl.textContent = state.humidity !== null && !isNaN(state.humidity) ? `${parseFloat(state.humidity).toFixed(1)} %` : '-- %';
        lightPercentValueEl.textContent = state.light_percent !== null ? `${state.light_percent} %` : '-- %';

        // Cập nhật điều khiển đèn
        const isRelayOn = state.relay_status === "ON";
        relayToggleSwitchEl.checked = isRelayOn;
        relayStatusTextEl.textContent = isRelayOn ? "BẬT" : "TẮT";
        relayStatusTextEl.style.color = isRelayOn ? 'var(--primary-color)' : '#777';

        // Cập nhật điều khiển chế độ
        currentModeTextEl.textContent = state.mode || '--';
        if (state.mode === "AUTO") {
            modeAutoBtnEl.classList.add('btn-mode-active');
            modeManualBtnEl.classList.remove('btn-mode-active');
        } else if (state.mode === "MANUAL") {
            modeManualBtnEl.classList.add('btn-mode-active');
            modeAutoBtnEl.classList.remove('btn-mode-active');
        }

        // Cập nhật điều khiển ngưỡng
        if (state.threshold !== null && parseInt(thresholdSliderEl.value) !== parseInt(state.threshold)) {
            thresholdSliderEl.value = state.threshold;
        }
        thresholdValueDisplayEl.textContent = thresholdSliderEl.value;
    }

    // --- Xử lý Sự kiện từ Người dùng ---

    relayToggleSwitchEl.addEventListener('change', () => {
        const newRelayState = relayToggleSwitchEl.checked ? "ON" : "OFF";
        sendCommandToBackend('/api/smarthome/relay/set', { command: newRelayState });
    });

    modeAutoBtnEl.addEventListener('click', () => {
        sendCommandToBackend('/api/smarthome/mode/set', { mode: "AUTO" });
    });

    modeManualBtnEl.addEventListener('click', () => {
        sendCommandToBackend('/api/smarthome/mode/set', { mode: "MANUAL" });
    });

    thresholdSliderEl.addEventListener('input', () => {
        thresholdValueDisplayEl.textContent = thresholdSliderEl.value;
    });

    setThresholdBtnEl.addEventListener('click', () => {
        const newThreshold = parseInt(thresholdSliderEl.value);
        sendCommandToBackend('/api/smarthome/threshold/set', { value: newThreshold });
    });


    // --- Chức năng giao tiếp với Backend ---

    // Hàm gửi lệnh đến backend qua API
    async function sendCommandToBackend(apiUrl, payload) {
        try {
            const response = await fetch(apiUrl, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload),
            });
            if (!response.ok) {
                console.error(`Lỗi khi gửi lệnh. Status: ${response.status}`);
                alert('Lỗi: Không thể gửi lệnh đến server.');
                loadInitialState(); // Tải lại trạng thái từ server để đồng bộ
            }
        } catch (error) {
            console.error('Lỗi mạng khi gửi lệnh:', error);
            alert('Lỗi mạng khi gửi lệnh.');
        }
    }

    // Hàm lấy trạng thái ban đầu từ backend
    async function loadInitialState() {
        try {
            const response = await fetch('/api/smarthome/state');
            if (response.ok) {
                const initialState = await response.json();
                console.log("Trạng thái ban đầu đã được tải:", initialState);
                updateUI(initialState);
            } else {
                console.error('Không thể tải trạng thái ban đầu. Status:', response.status);
            }
        } catch (error) {
            console.error('Lỗi khi tải trạng thái ban đầu:', error);
        }
    }

    // Hàm kết nối WebSocket để nhận dữ liệu thời gian thực
    function connectWebSocket() {
        // WebSocket URL sẽ tự động dùng host và port của trang web
        const socket = new WebSocket(`ws://${window.location.host}`);

        socket.onopen = () => {
            console.log('✅ Kết nối WebSocket đã được thiết lập.');
            mqttStatusIndicatorEl.className = 'status-dot status-connected';
            mqttStatusTextEl.textContent = 'Đã kết nối';
        };

        socket.onmessage = (event) => {
            try {
                const serverState = JSON.parse(event.data);
                console.log('Nhận được dữ liệu WebSocket:', serverState);
                updateUI(serverState); // Cập nhật giao diện với dữ liệu mới
            } catch (e) {
                console.error("Lỗi parse dữ liệu WebSocket hoặc cập nhật UI", e);
            }
        };

        socket.onclose = () => {
            console.log('Kết nối WebSocket đã đóng. Đang thử kết nối lại sau 3 giây...');
            mqttStatusIndicatorEl.className = 'status-dot status-disconnected';
            mqttStatusTextEl.textContent = 'Đã mất kết nối';
            setTimeout(connectWebSocket, 3000); // Thử kết nối lại sau 3 giây
        };

        socket.onerror = (error) => {
            console.error('Lỗi WebSocket:', error);
            mqttStatusIndicatorEl.className = 'status-dot status-connecting';
            mqttStatusTextEl.textContent = 'Đang kết nối...';
            // Hàm onclose sẽ tự động xử lý việc kết nối lại
        };
    }

    // --- Khởi tạo ứng dụng ---
    loadInitialState(); // Tải trạng thái lần đầu
    connectWebSocket(); // Bắt đầu kết nối WebSocket
});