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
    if (data.fault) {
        FaultIndicator.textContent = "Critical: fault detected! Stop operations immediately";
        FaultIndicator.classList.add('fault-active');
    } else {
        FaultIndicator.textContent = "System operating as normal.";
        FaultIndicator.classList.remove('fault-active');
    }
};

// handle motor speed changes 

const speedSlider = document.getElementById('motor-speed');
const speedVal = document.getElementById('speed-val');

speedSlider.addEventListener('input', (e) => {
    speedVal.innerText = e.target.value;
    ws.send(JSON.stringify({command: 'speed', value: parseInt(e.target.value)}));
});