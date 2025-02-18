const canvas = document.getElementById('heatmap');
const ctx = canvas.getContext('2d');
canvas.width = document.querySelector('.map-container').clientWidth;
canvas.height = document.querySelector('.map-container').clientHeight;

let mapData = []; 
const gridSize = 10; 
const cols = Math.floor(canvas.width / gridSize);
const rows = Math.floor(canvas.height / gridSize);

function drawHeatmap(sensorType) {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    mapData.forEach(point => {
        const x = Math.floor(point.x / gridSize);
        const y = Math.floor(point.y / gridSize);
        const value = point[sensorType];

        let color;
        if (sensorType === 'temperature') {
            const intensity = Math.min(255, Math.max(0, (value - 20) * 10)); 
            color = `rgba(255, ${255 - intensity}, 0, 0.8)`;
        } else if (sensorType === 'humidity') {
            const intensity = Math.min(255, Math.max(0, value * 2.55)); 
            color = `rgba(0, 0, ${intensity}, 0.8)`;
        } else if (sensorType === 'gas') {
            const intensity = Math.min(255, Math.max(0, value * 0.5)); 
            color = `rgba(${intensity}, 0, 0, 0.8)`;
        } else if (sensorType === 'luminosity') {
            const intensity = Math.min(255, Math.max(0, value * 0.5)); 
            color = `rgba(${intensity}, ${intensity}, 0, 0.8)`;
        }

        ctx.fillStyle = color;
        ctx.fillRect(x * gridSize, y * gridSize, gridSize, gridSize);
    });
}

function updateMap(data) {
    const distance = data.distance;
    const angle = data.angle || 0; 

    const newX = Math.min(canvas.width, Math.max(0, distance * Math.cos(angle)));
    const newY = Math.min(canvas.height, Math.max(0, distance * Math.sin(angle)));

    mapData.push({
        x: newX,
        y: newY,
        temperature: data.temperature,
        humidity: data.humidity,
        gas: data.gas,
        luminosity: data.luminosity
    });

    const selectedSensor = document.querySelector('input[name="sensor"]:checked').value;
    drawHeatmap(selectedSensor);
}

function updateSidebar(data) {
    document.getElementById('temperature').textContent = data.temperature + " °C";
    document.getElementById('humidity').textContent = data.humidity + " %";
    document.getElementById('gas').textContent = data.gas;
    document.getElementById('luminosity').textContent = data.luminosity;
}

setInterval(() => {
    fetch('/data') 
        .then(response => response.json())
        .then(data => {
            updateSidebar(data);
            updateMap(data);
        });
}, 1000);

window.onload = () => drawHeatmap('temperature');