#ifndef ICHIP_ADAPTER_H
#define ICHIP_ADAPTER_H

#include <QString>

#include <cstdint>
#include <string>
#include <vector>

// Параметры чипа EEPROM, нужные адаптеру для корректных I2C-транзакций
struct ChipParams {
    uint8_t deviceAddress = 0x50;   // 7-битный I2C-адрес (0x50..0x57)
    uint32_t chipSize = 2048;       // размер массива в байтах
    uint16_t pageSize = 16;         // размер страницы записи
    bool twoByteAddress = false;    // 2 байта адреса памяти (24C32 и крупнее)
    uint8_t blockBits = 0;          // биты адреса в байте устройства (24C04/08/16)
    uint32_t writeCycleMs = 5;      // типовой цикл записи
};

// Интерфейс адаптера программатора (чип через I2C/SPI)
class IChipAdapter
{
public:
    virtual ~IChipAdapter() = default;

    // Открыть соединение с программатором
    virtual bool open() = 0;

    // Закрыть соединение
    virtual void close() = 0;

    // Проверка, открыт ли адаптер
    virtual bool isOpen() const = 0;

    // Прочитать length байт с адреса address
    virtual std::vector<uint8_t> read(uint32_t address, uint32_t length) = 0;

    // Записать данные по адресу address (страницы режет адаптер)
    virtual bool write(uint32_t address, const std::vector<uint8_t> &data) = 0;

    // Последняя ошибка
    virtual std::string lastError() const = 0;

    // Имя адаптера (для отображения в UI)
    virtual QString adapterName() const = 0;

    // Параметры чипа (адресация, страница, цикл записи)
    virtual void setChipParams(const ChipParams &params) = 0;

    // Просканировать шину и вернуть ACK-нувшие 7-битные адреса
    virtual std::vector<uint8_t> scanBus() = 0;
};

#endif // ICHIP_ADAPTER_H