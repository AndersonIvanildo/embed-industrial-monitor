const express = require('express');
const bodyParser = require('body-parser');
const WebSocket = require('ws');
const app = express();
const port = 8080;

app.use(bodyParser.json());

// Cria um servidor HTTP para o Express
const server = app.listen(port, () => {
    console.log(`Servidor rodando em http://localhost:${port}`);
});

// Cria um servidor WebSocket
const wss = new WebSocket.Server({ server });

// Armazena os dados recebidos
let sensorData = {};

// Rota para receber dados da ESP32
app.post('/data', (req, res) => {
    sensorData = req.body;
    console.log('Dados recebidos:', sensorData);

    // Envia os dados para todos os clientes WebSocket conectados
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify(sensorData));
        }
    });

    res.status(200).send('Dados recebidos com sucesso!');
});

// Rota para servir o front-end
app.get('/', (req, res) => {
    res.sendFile(__dirname + 'src/web_service_project/front-end/index.html');
});

// Servir arquivos estáticos (CSS, JS, etc.)
app.use(express.static('public'));