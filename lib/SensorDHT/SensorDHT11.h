#ifndef SENSORDHT11_H
#define SENSORDHT11_H

#include <DHT.h>

class SensorDHT {
    public:
        SensorDHT(uint8_t pinoDados);
        float readTemperature();
        float readHumidity();
    
    private:
        uint8_t _pino;
        DHT _dht;
};

#endif