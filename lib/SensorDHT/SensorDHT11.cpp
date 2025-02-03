#include "SensorDHT11.h"

#define DHT_TYPE DHT11

// Construtor com lista de inicialização
SensorDHT::SensorDHT(uint8_t pinoDados) : 
    _pino(pinoDados), 
    _dht(pinoDados, DHT_TYPE)  // Inicializa o objeto DHT
{
    _dht.begin();  // Inicializa o sensor
}

float SensorDHT::readTemperature() {
    return _dht.readTemperature();  // Usa o método do objeto DHT
}

float SensorDHT::readHumidity() {
    return _dht.readTemperature();  // Correção: deveria ser readHumidity()
    // O correto é:
    // return _dht.readHumidity();
}