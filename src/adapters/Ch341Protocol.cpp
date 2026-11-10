#include "Ch341Protocol.h"

#include <algorithm>
#include <cassert>

namespace {

uint8_t blockMask(uint8_t blockBits)
{
    if (blockBits == 0) {
        return 0;
    }
    return static_cast<uint8_t>(((1u << blockBits) - 1u) << 1);
}

uint8_t blockValue(const Ch341Addressing &addressing, uint32_t address)
{
    if (addressing.blockBits == 0) {
        return 0;
    }

    // Для 24C32+ старшие биты адреса находятся выше 16-го бита,
    // для 24C04/08/16 — выше 8-го.
    const int shift = addressing.twoByteAddress ? 16 : 8;
    const uint32_t mask = (1u << addressing.blockBits) - 1u;
    return static_cast<uint8_t>((address >> shift) & mask);
}

void appendOut(std::vector<uint8_t> &packet, const std::vector<uint8_t> &bytes)
{
    assert(bytes.size() <= 0x3F);
    packet.push_back(static_cast<uint8_t>(Ch341Protocol::kOut | bytes.size()));
    packet.insert(packet.end(), bytes.begin(), bytes.end());
}

} // namespace

namespace Ch341Protocol {

uint8_t deviceAddressByte(const Ch341Addressing &addressing, uint32_t address, bool readOp)
{
    uint8_t addr = static_cast<uint8_t>((addressing.deviceAddress << 1) & 0xFE);
    addr = static_cast<uint8_t>(addr & ~blockMask(addressing.blockBits));
    addr = static_cast<uint8_t>(addr | (blockValue(addressing, address) << 1));
    if (readOp) {
        addr = static_cast<uint8_t>(addr | 0x01);
    }
    return addr;
}

std::vector<uint8_t> memoryAddressBytes(const Ch341Addressing &addressing, uint32_t address)
{
    if (addressing.twoByteAddress) {
        return {static_cast<uint8_t>((address >> 8) & 0xFF),
                static_cast<uint8_t>(address & 0xFF)};
    }
    return {static_cast<uint8_t>(address & 0xFF)};
}

std::vector<uint8_t> buildConfigure(uint8_t speed)
{
    return {kStreamCommand, static_cast<uint8_t>(kSet | (speed & 0x03)), kEnd};
}

std::vector<uint8_t> buildProbe(const Ch341Addressing &addressing, uint32_t address)
{
    std::vector<uint8_t> packet;
    packet.reserve(6);
    packet.push_back(kStreamCommand);
    packet.push_back(kStart);
    packet.push_back(static_cast<uint8_t>(kOut | 1));
    packet.push_back(deviceAddressByte(addressing, address, false));
    packet.push_back(kStop);
    packet.push_back(kEnd);
    return packet;
}

std::vector<uint8_t> buildRead(const Ch341Addressing &addressing, uint32_t address,
                               uint16_t length)
{
    assert(length > 0 && length <= 64);

    const std::vector<uint8_t> memAddr = memoryAddressBytes(addressing, address);

    std::vector<uint8_t> packet;
    packet.reserve(memAddr.size() + 10);
    packet.push_back(kStreamCommand);

    // Dummy write: адрес устройства (write) + адрес памяти.
    packet.push_back(kStart);
    std::vector<uint8_t> first;
    first.reserve(memAddr.size() + 1);
    first.push_back(deviceAddressByte(addressing, address, false));
    first.insert(first.end(), memAddr.begin(), memAddr.end());
    appendOut(packet, first);

    // Repeated START и чтение.
    packet.push_back(kStart);
    packet.push_back(static_cast<uint8_t>(kOut | 1));
    packet.push_back(deviceAddressByte(addressing, address, true));
    packet.push_back(static_cast<uint8_t>(kIn | static_cast<uint8_t>(length - 1)));
    packet.push_back(kStop);
    packet.push_back(kEnd);
    return packet;
}

std::vector<uint8_t> buildWrite(const Ch341Addressing &addressing, uint32_t address,
                                const uint8_t *data, uint16_t length)
{
    assert(data != nullptr || length == 0);

    const std::vector<uint8_t> memAddr = memoryAddressBytes(addressing, address);
    std::vector<uint8_t> payload;
    payload.reserve(memAddr.size() + length + 1);
    payload.push_back(deviceAddressByte(addressing, address, false));
    payload.insert(payload.end(), memAddr.begin(), memAddr.end());
    if (length > 0) {
        payload.insert(payload.end(), data, data + length);
    }

    std::vector<uint8_t> packet;
    packet.reserve(payload.size() + 5);
    packet.push_back(kStreamCommand);
    packet.push_back(kStart);
    appendOut(packet, payload);
    packet.push_back(kStop);
    packet.push_back(kEnd);
    return packet;
}

std::vector<uint8_t> buildBusRecovery()
{
    return {kStreamCommand, kStop, kEnd};
}

bool responseIsAck(const std::vector<uint8_t> &response)
{
    return !response.empty() && (response.front() & kStatusNak) == 0;
}

} // namespace Ch341Protocol
