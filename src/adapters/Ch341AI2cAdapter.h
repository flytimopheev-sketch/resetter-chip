#ifndef CH341A_I2C_ADAPTER_H
#define CH341A_I2C_ADAPTER_H

#include "Ch341Protocol.h"
#include "IChipAdapter.h"

#include <libusb-1.0/libusb.h>

#include <cstdint>
#include <mutex>
#include <vector>

// Адаптер CH341A (I2C-режим) через libusb-1.0.
// Поддерживает оригинальные CH341A и совместимые самодельные платы:
//  - автоматический поиск по известным PID;
//  - явный VID/PID, если у платы изменён идентификатор.
class Ch341AI2cAdapter : public IChipAdapter
{
public:
    explicit Ch341AI2cAdapter(uint16_t vendorId = 0, uint16_t productId = 0);
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
    uint16_t m_vendorId = 0;
    uint16_t m_productId = 0;
    uint16_t m_openedVendorId = 0;
    uint16_t m_openedProductId = 0;
    bool m_kernelDriverDetached = false;

    ChipParams m_params;
    bool m_opened = false;
    mutable std::mutex m_mutex;
    std::string m_lastError;

    Ch341Addressing addressing() const;

    void closeUnlocked();

    bool configureI2cUnlocked();

    // Выполнить пакет команд потока; вернуть прочитанные байты.
    // Для write-only пакетов readLength == 0, но статус всё равно читается.
    bool streamUnlocked(const std::vector<uint8_t> &packet,
                        size_t readLength,
                        std::vector<uint8_t> &out);

    bool writeChunkUnlocked(uint32_t address, const uint8_t *data, size_t length);
    bool readChunkUnlocked(uint32_t address, uint8_t *dst, size_t length);

    bool probeUnlocked(uint32_t address);
    void recoverBusUnlocked();
    void waitForWriteCycleUnlocked();

    void setError(const std::string &message);
};

#endif // CH341A_I2C_ADAPTER_H
