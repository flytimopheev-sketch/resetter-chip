#include <QTest>

#include "adapters/Ch341Protocol.h"
#include "adapters/SerialProtocol.h"

#include <vector>

class TestProtocol : public QObject
{
    Q_OBJECT

private slots:
    void ch341DeviceAddress24C16()
    {
        Ch341Addressing addressing;
        addressing.deviceAddress = 0x50;
        addressing.twoByteAddress = false;
        addressing.blockBits = 3;

        QCOMPARE(Ch341Protocol::deviceAddressByte(addressing, 0x0000, false),
                 static_cast<uint8_t>(0xA0));
        QCOMPARE(Ch341Protocol::deviceAddressByte(addressing, 0x0100, false),
                 static_cast<uint8_t>(0xA2));
        QCOMPARE(Ch341Protocol::deviceAddressByte(addressing, 0x0700, true),
                 static_cast<uint8_t>(0xAF));
    }

    void ch341DeviceAddress24C1024()
    {
        Ch341Addressing addressing;
        addressing.deviceAddress = 0x50;
        addressing.twoByteAddress = true;
        addressing.blockBits = 1;

        QCOMPARE(Ch341Protocol::deviceAddressByte(addressing, 0x00000, false),
                 static_cast<uint8_t>(0xA0));
        QCOMPARE(Ch341Protocol::deviceAddressByte(addressing, 0x10000, false),
                 static_cast<uint8_t>(0xA2));
        QCOMPARE(Ch341Protocol::deviceAddressByte(addressing, 0x10000, true),
                 static_cast<uint8_t>(0xA3));

        const std::vector<uint8_t> mem = Ch341Protocol::memoryAddressBytes(addressing, 0x12345);
        QCOMPARE(static_cast<int>(mem.size()), 2);
        QCOMPARE(mem[0], static_cast<uint8_t>(0x23));
        QCOMPARE(mem[1], static_cast<uint8_t>(0x45));
    }

    void ch341ReadPacket()
    {
        Ch341Addressing addressing;
        addressing.deviceAddress = 0x50;
        addressing.twoByteAddress = false;
        addressing.blockBits = 3;

        const std::vector<uint8_t> packet =
            Ch341Protocol::buildRead(addressing, 0x0100, 4);
        const std::vector<uint8_t> expected = {
            0xAA, 0x74, 0x82, 0xA2, 0x00,
            0x74, 0x81, 0xA3, 0xC3, 0x75, 0x00
        };
        QVERIFY(packet == expected);
    }

    void ch341WritePacket()
    {
        Ch341Addressing addressing;
        addressing.deviceAddress = 0x50;
        addressing.twoByteAddress = false;
        addressing.blockBits = 3;

        const uint8_t data[] = {0x01, 0x02, 0x03};
        const std::vector<uint8_t> packet =
            Ch341Protocol::buildWrite(addressing, 0x0020, data, 3);
        const std::vector<uint8_t> expected = {
            0xAA, 0x74, 0x85, 0xA0, 0x20, 0x01, 0x02, 0x03, 0x75, 0x00
        };
        QVERIFY(packet == expected);
    }

    void serialRequestLayout()
    {
        const QByteArray frame =
            SerialProtocol::buildRequest(SerialProtocol::Command::Read, 0x50, 0x12345678, 16);

        QCOMPARE(frame.size(), 12);
        QCOMPARE(static_cast<quint8>(frame.at(0)), static_cast<quint8>(0xA5));
        QCOMPARE(static_cast<quint8>(frame.at(1)), static_cast<quint8>(0x5A));
        QCOMPARE(static_cast<quint8>(frame.at(2)),
                 static_cast<quint8>(SerialProtocol::Command::Read));
        QCOMPARE(static_cast<quint8>(frame.at(3)), static_cast<quint8>(0x50));
        QCOMPARE(static_cast<quint8>(frame.at(4)), static_cast<quint8>(0x78));
        QCOMPARE(static_cast<quint8>(frame.at(5)), static_cast<quint8>(0x56));
        QCOMPARE(static_cast<quint8>(frame.at(6)), static_cast<quint8>(0x34));
        QCOMPARE(static_cast<quint8>(frame.at(7)), static_cast<quint8>(0x12));
        QCOMPARE(static_cast<quint8>(frame.at(8)), static_cast<quint8>(0x10));
        QCOMPARE(static_cast<quint8>(frame.at(9)), static_cast<quint8>(0x00));

        const quint16 checksum = SerialProtocol::checksum16(frame, 2, frame.size() - 2);
        QCOMPARE(static_cast<quint8>(frame.at(10)), static_cast<quint8>(checksum & 0xFF));
        QCOMPARE(static_cast<quint8>(frame.at(11)), static_cast<quint8>((checksum >> 8) & 0xFF));
    }

    void serialResponseParse()
    {
        QByteArray frame;
        frame.append(static_cast<char>(SerialProtocol::kMagic1));
        frame.append(static_cast<char>(SerialProtocol::kMagic2));
        frame.append(static_cast<char>(SerialProtocol::Status::Ok));
        frame.append(static_cast<char>(SerialProtocol::Command::Read));
        frame.append(static_cast<char>(3));
        frame.append(static_cast<char>(0));
        frame.append(QByteArray("\xAA\xBB\xCC", 3));
        const quint16 checksum =
            SerialProtocol::checksum16(frame, 2, frame.size());
        frame.append(static_cast<char>(checksum & 0xFF));
        frame.append(static_cast<char>((checksum >> 8) & 0xFF));

        SerialProtocol::Status status = SerialProtocol::Status::BusError;
        SerialProtocol::Command command = SerialProtocol::Command::Ping;
        QByteArray payload;
        QString error;
        QVERIFY2(SerialProtocol::parseResponse(frame, &status, &command, &payload, &error),
                 qPrintable(error));
        QCOMPARE(static_cast<int>(status), static_cast<int>(SerialProtocol::Status::Ok));
        QCOMPARE(static_cast<int>(command), static_cast<int>(SerialProtocol::Command::Read));
        QCOMPARE(payload, QByteArray("\xAA\xBB\xCC", 3));
    }

    void serialResponseRejectsBadChecksum()
    {
        QByteArray frame;
        frame.append(static_cast<char>(SerialProtocol::kMagic1));
        frame.append(static_cast<char>(SerialProtocol::kMagic2));
        frame.append(static_cast<char>(SerialProtocol::Status::Ok));
        frame.append(static_cast<char>(SerialProtocol::Command::Ping));
        frame.append(static_cast<char>(0));
        frame.append(static_cast<char>(0));
        frame.append(static_cast<char>(0x00));
        frame.append(static_cast<char>(0x00));

        SerialProtocol::Status status;
        SerialProtocol::Command command;
        QByteArray payload;
        QVERIFY(!SerialProtocol::parseResponse(frame, &status, &command, &payload));
    }

    void serialConfigurePayload()
    {
        SerialProtocol::ChipConfig config;
        config.deviceAddress = 0x50;
        config.twoByteAddress = true;
        config.blockBits = 1;
        config.pageSize = 256;
        config.writeCycleMs = 5;

        const QByteArray payload = SerialProtocol::configurePayload(config);
        QCOMPARE(payload.size(), 7);
        QCOMPARE(static_cast<quint8>(payload.at(0)), static_cast<quint8>(0x50));
        QCOMPARE(static_cast<quint8>(payload.at(1)), static_cast<quint8>(1));
        QCOMPARE(static_cast<quint8>(payload.at(2)), static_cast<quint8>(1));
        QCOMPARE(static_cast<quint8>(payload.at(3)), static_cast<quint8>(0x00));
        QCOMPARE(static_cast<quint8>(payload.at(4)), static_cast<quint8>(0x01));
        QCOMPARE(static_cast<quint8>(payload.at(5)), static_cast<quint8>(0x05));
        QCOMPARE(static_cast<quint8>(payload.at(6)), static_cast<quint8>(0x00));
    }
};

QTEST_MAIN(TestProtocol)
#include "test_protocol.moc"
