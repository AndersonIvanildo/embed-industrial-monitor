// server.js (substitua o código existente)
const express = require('express');
const { SerialPort } = require('serialport');
const app = express();
const port = 8080;

// Configuração da porta serial
const portPath = '/dev/ttyUSB0'; // Linux
// const portPath = 'COM3'; // Windows
const serialPort = new SerialPort({ path: portPath, baudRate: 115200 });

let sensorData = {};

serialPort.on('data', (data) => {
  try {
    const jsonString = data.toString().trim();
    sensorData = JSON.parse(jsonString);
    console.log('Dados recebidos:', sensorData);
  } catch (error) {
    console.error('Erro ao parsear JSON:', error);
  }
});

// Rota para fornecer dados ao front-end (GET)
app.get('/data', (req, res) => {
    res.json(sensorData); // Retorna os dados armazenados como JSON
});

// Rota para servir o front-end (GET /)
app.get('/', (req, res) => {
    res.sendFile("/home/anderson/Documents/codes/cppProjects/embed-industrial-monitor/src/web_service_project/front-end/index.html");
});

// Servir arquivos estáticos (CSS, JS, etc.)
app.use(express.static("/home/anderson/Documents/codes/cppProjects/embed-industrial-monitor/src/web_service_project/front-end"));

// Cria um servidor HTTP para o Express
const server = app.listen(port, () => {
    console.log(`Servidor rodando em http://localhost:${port}`);
});

// Cria um servidor WebSocket
const wss = new WebSocket.Server({ server });

// Tratamento de eventos WebSocket
wss.on('connection', (ws) => {
    console.log('Novo cliente conectado');

    ws.on('error', (error) => {
        console.error('Erro no WebSocket:', error);
    });

    ws.on('close', () => {
        console.log('Cliente desconectado');
    });
});