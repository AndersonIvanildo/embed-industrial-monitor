// Função para atualizar os valores dos sensores na interface
function updateSidebar(data) {
    document.getElementById('temperature').textContent = data.temperature || '--';
    document.getElementById('humidity').textContent = data.humidity || '--';
    document.getElementById('gas').textContent = data.gas || '--';
    document.getElementById('luminosity').textContent = data.luminosity || '--';
}

// Conexão WebSocket para receber dados em tempo real
const socket = new WebSocket(`ws://${window.location.hostname}:8080`);

socket.onopen = () => {
    console.log('Conexão WebSocket estabelecida');
};

socket.onmessage = (event) => {
    const data = JSON.parse(event.data);
    updateSidebar(data);
};

socket.onerror = (error) => {
    console.error('Erro no WebSocket:', error);
};

socket.onclose = () => {
    console.log('Conexão WebSocket fechada');
};

// Fallback para requisições HTTP caso o WebSocket falhe
setInterval(() => {
    fetch('/data')
        .then(response => response.json())
        .then(data => {
            updateSidebar(data);
        })
        .catch(error => {
            console.error('Erro ao buscar dados via HTTP:', error);
        });
}, 1000); // Atualiza a cada 5 segundos