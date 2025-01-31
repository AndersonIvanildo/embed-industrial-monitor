#include <Wire.h>
#include <Arduino.h>

// Endereço I2C do MPU9250
#define MPU9250_ADDRESS 0x69

// Registradores importantes
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B
#define GYRO_XOUT_H  0x43

// teste

// Função para inicializar o MPU9250
void initMPU9250() {
    Wire.beginTransmission(MPU9250_ADDRESS);
    Wire.write(PWR_MGMT_1);
    Wire.write(0x00);  // Sai do modo de suspensão
    Wire.endTransmission();
    delay(100);
}

// Função para ler dois bytes de um registrador
int16_t readWord(uint8_t reg) {
    Wire.beginTransmission(MPU9250_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU9250_ADDRESS, 2);
    if (Wire.available() >= 2) {
        int16_t high = Wire.read();
        int16_t low = Wire.read();
        return (high << 8) | low;
    }
    return 0;
}

// Função para ler aceleração (X, Y, Z)
void readAccelerometer(float &ax, float &ay, float &az) {
    ax = readWord(ACCEL_XOUT_H) / 16384.0; // Divisor para escala padrão ±2g
    ay = readWord(ACCEL_XOUT_H + 2) / 16384.0;
    az = readWord(ACCEL_XOUT_H + 4) / 16384.0;
}

// Função para ler giroscópio (X, Y, Z)
void readGyroscope(float &gx, float &gy, float &gz) {
    gx = readWord(GYRO_XOUT_H) / 131.0; // Divisor para escala padrão ±250°/s
    gy = readWord(GYRO_XOUT_H + 2) / 131.0;
    gz = readWord(GYRO_XOUT_H + 4) / 131.0;
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    delay(1000);

    // Inicializa o MPU9250
    initMPU9250();
    Serial.println("MPU9250 inicializado!");
}

void loop() {
    float ax, ay, az;
    float gx, gy, gz;

    // Lê os valores do acelerômetro
    readAccelerometer(ax, ay, az);
    // Lê os valores do giroscópio
    readGyroscope(gx, gy, gz);

    // Imprime os resultados no Serial Monitor
    Serial.print("Aceleração (g) -> X: ");
    Serial.print(ax, 2);
    Serial.print(" | Y: ");
    Serial.print(ay, 2);
    Serial.print(" | Z: ");
    Serial.println(az, 2);

    Serial.print("Giroscópio (°/s) -> X: ");
    Serial.print(gx, 2);
    Serial.print(" | Y: ");
    Serial.print(gy, 2);
    Serial.print(" | Z: ");
    Serial.println(gz, 2);

    Serial.println("---------------------------------------");
    delay(500); // Aguarda 500ms para a próxima leitura
}
