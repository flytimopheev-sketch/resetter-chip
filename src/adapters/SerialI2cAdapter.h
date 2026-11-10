#ifndef SERIAL_I2C_ADAPTER_H
#define SERIAL_I2C_ADAPTER_H

#include "IChipAdapter.h"
#include "SerialProtocol.h"

#include <QString>

#include <cstdint>
#include <mutex>
#include <string>

// Адаптер самодельного I2C-программатора на Arduino/ESP32/ESP8266.
// Общается по последовательному порту бинарным протоколом SerialProtocol.
// Прошивка: firmware/resetter-serial-i2c/resetter-serial-i2c.ino
class SerialI2cAdapter : public IChipAdapter
{
public:
    explicit SerialI2cAdapter(const QString &portName = QString());
    ~SerialI2cAdapter() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;

    std::vector<uint8_t> read(uint32_t address, uint32_t length) override;
    bool write(uint32_t address, const std::vector<uint8_t> &data) override;

    std::string lastError() const override;
    QString adapterName() const override;

    void setChipParams(const ChipParams &params) override;
    std::vector<uint8_t> scanBus() override;

private:
    QString m_portName;
    int m_fd = -1;
    ChipParams m_params;
    bool m_configured = false;
    mutable std::mutex m_mutex;
    std::string m_lastError;

    void closeUnlocked();
    void setError(const std::string &message);

    SerialProtocol::ChipConfig chipConfig() const;
    bool ensureConfiguredUnlocked();

    bool sendRequestUnlocked(SerialProtocol::Command command, quint8 i2cAddress,
                             quint32 memoryAddress, quint16 length,
                             const QByteArray &payload, QByteArray &responsePayload);

    bool writeAllUnlocked(const QByteArray &data);
    bool readExactUnlocked(char *buffer, int size, int timeoutMs);
};

#endif // SERIAL_I2C_ADAPTER_H
