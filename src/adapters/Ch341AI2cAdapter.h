#ifndef CH341A_I2C_ADAPTER_H
#define CH341A_I2C_ADAPTER_H

#include "IChipAdapter.h"

#include <libusb-1.0/libusb.h>

#include <mutex>
#include <vector>

// Адаптер CH341A (I2C-режим) через libusb-1.0.
// Протокол обмена соответствует режиму «I2C stream» микросхемы CH341A:
//   ведущая команда 0xAA, далее подкоманды потока:
//     0x74 START, 0x75 STOP, 0x80|n запись n байт,
//     0xC0|(n-1) чтение n байт, 0x60|speed скорость, 0x00 конец пакета.
// Пакет передаётся bulk-передачей на endpoint 0x02, данные читаются с 0x82.
class Ch341AI2cAdapter : public IChipAdapter
{
public:
    Ch341AI2cAdapter();
    ~Ch341AI2cAdapter() override;

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
    libusb_context *m_usbCtx = nullptr;
    libusb_device_handle *m_handle = nullptr;

    ChipParams m_params;
    bool m_opened = false;
    mutable std::mutex m_mutex;
    std::string m_lastError;

    // Закрыть без повторного захвата мьютекса
    void closeUnlocked();

    // Включить I2C-режим и выставить скорость шины
    bool configureI2cUnlocked();

    // Выполнить пакет команд потока; вернуть прочитанные байты
    bool streamUnlocked(const std::vector<uint8_t> &packet,
                        size_t readLength,
                        std::vector<uint8_t> &out);

    // Адресный байт устройства с учётом битов блока (24C04/08/16)
    uint8_t deviceAddressByte(uint32_t address, bool readOp) const;

    // Байты адреса памяти (1 или 2)
    std::vector<uint8_t> memoryAddressBytes(uint32_t address) const;

    // Транзакция записи одной страницы
    bool writeChunkUnlocked(uint32_t address, const uint8_t *data, size_t length);

    // Транзакция чтения
    bool readChunkUnlocked(uint32_t address, uint8_t *dst, size_t length);

    void setError(const std::string &message);
};

#endif // CH341A_I2C_ADAPTER_H
