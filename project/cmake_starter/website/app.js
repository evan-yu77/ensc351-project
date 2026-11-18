// JavaScript script for the dashboard, draws charts and sets up the WebSocket


const ctxVib = document.getElementById('vibrationChart').getContext('2d');
const vibChart = new Chart(ctxVib, {
    type: 'line',
    data: { labels: [], datasets: [
        { label: 'Accel X', borderColor: '#ff5252', data: [], tension: 0.2 },
        { label: 'Accel Y', borderColor: '#448aff', data: [], tension: 0.2 }
    ]},
    options: { animation: false, scales: { x: { display: false } } }
});

// fault information display
const FAULT_INFO = {
    1: {
        title: "[Code 1] MOTOR STALLED (High Current, Low RPM)",
        tips: [
            "DANGER: Check for physical jams or tangles on the motor shaft.",
            "Verify the encoder disk hasn't slipped off.",
            "PWM duty cycle may be too low for the current load."
        ]
    },
    2: {
        title: "[Code 2] OVERCURRENT SPIKE (> 1000 mA)",
        tips: [
            "DANGER: Check for exposed wires shorting at AO1/AO2.",
            "Inspect the TB6612FNG driver for overheating.",
            "Verify the INA219 I2C calibration math."
        ]
    },
    3: {
        title: "[Code 3] SEVERE VIBRATION / IMBALANCE",
        tips: [
            "DANGER: Check if motor mount screws have vibrated loose.",
            "Verify the rotor is centered on the shaft.",
            "Ensure the accelerometer is taped down securely."
        ]
    }
};

// display current and RPM statistics 
const ctxMotor = document.getElementById('motorChart').getContext('2d');
const motorChart = new Chart(ctxMotor, {
    type: 'line',
    data: { labels: [], datasets: [
        { label: 'Current (mA)', borderColor: '#ffeb3b', data: [], tension: 0.2 },
        { label: 'RPM', borderColor: '#69f0ae', data: [], tension: 0.2 }
    ]},
    options: { animation: false, scales: { x: { display: false } } }
});

// connect WebSocket to python server (using standard port 8080)
const ws = new WebSocket(`ws://${window.location.hostname}:8080/ws`);

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);

    const time = new Date().toLocaleTimeString();
    vibChart.data.labels.push(time);
    vibChart.data.datasets[0].data.push(data.accel_x);
    vibChart.data.datasets[1].data.push(data.accel_y);

    motorChart.data.labels.push(time);
    motorChart.data.datasets[0].data.push(data.current_ma);
    motorChart.data.datasets[1].data.push(data.rpm);

    if (vibChart.data.labels.length > 75) {
        vibChart.data.labels.shift();
        vibChart.data.datasets.forEach(ds => ds.data.shift());
        motorChart.data.labels.shift();
        motorChart.data.datasets.forEach(ds => ds.data.shift());
    }

    vibChart.update();
    motorChart.update();

    const FaultIndicator = document.getElementById('fault-indicator');
    const FaultTips = document.getElementById('fault-tips');
    if (data.fault && FAULT_INFO[data.fault_code]) {
        const info = FAULT_INFO[data.fault_code];
        faultIndicator.textContent = `FAULT DETECTED: ${info.title}`;
        faultIndicator.classList.add('fault-active');

        faultTips.innerHTML = `<strong>Debugging Tips:</strong><ul style="margin: 6px 0 0 18px; padding: 0;">` +
            info.tips.map(tip => `<li>${tip}</li>`).join('') + `</ul>`;
        faultTips.style.display = 'block';

        // Sync slider to 0 since C daemon halted the motor
        speedSlider.value = 0;
        speedVal.innerText = 0;
    } else {
        FaultIndicator.textContent = "System operating as normal.";
        FaultIndicator.classList.remove('fault-active');
        FaultTips.style.display = 'none';
    }
};

// handle motor speed changes 

const speedSlider = document.getElementById('motor-speed');
const speedVal = document.getElementById('speed-val');

speedSlider.addEventListener('input', (e) => {
    speedVal.innerText = e.target.value;
    ws.send(JSON.stringify({command: 'speed', value: parseInt(e.target.value)}));
});

// reset button 
document.getElementById('reset-btn').addEventListener('click', () => {
    ws.send(JSON.stringify({ command: 'reset' }));
});

// enable or disable system

document.getElementById('fault-toggle').addEventListener('change', (e) => {
    ws.send(JSON.stringify({ command: 'fault_toggle', enabled: e.target.checked }));
});