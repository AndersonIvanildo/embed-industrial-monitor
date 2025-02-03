#ifdef SENSORDHT11_H
#define SENSORDHT11_H

class SensorDHT {
    public:
        SensorDHT(int pinoDados);
        void returnTemperature();
        void returnHumidity();

    private:
    //existe?
}

#endif