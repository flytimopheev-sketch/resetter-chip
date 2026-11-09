#include <QCoreApplication>
#include <QTest>
#include <QByteArray>
#include "../src/core/Checksum.h"

#include <cstring>

class TestChecksum : public QObject
{
    Q_OBJECT
private slots:
    void testCRC32Empty()
    {
        QByteArray empty;
        QCOMPARE(Checksum::crc32(empty), static_cast<quint32>(0x00000000));
    }

    void testCRC32Known()
    {
        QByteArray data("123456789", 9);
        // CRC32 для "123456789" = 0xCBF43926
        QCOMPARE(Checksum::crc32(data), static_cast<quint32>(0xCBF43926));
    }

    void testAppendAndVerifyCRC32()
    {
        QByteArray data("test data");
        QByteArray withCRC = Checksum::appendCRC32(data);
        QVERIFY(Checksum::verifyCRC32(withCRC));

        // Портим данные — верификация должна упасть
        QByteArray corrupted = withCRC;
        corrupted[0] ^= 0xFF;
        QVERIFY(!Checksum::verifyCRC32(corrupted));
    }

    void testSimpleSum()
    {
        QByteArray data("\x01\x02\x03\x04", 4);
        QCOMPARE(Checksum::simpleSum(data), static_cast<quint16>(0x000A));
    }

    void testComputeAlgorithm()
    {
        QByteArray data("test");
        QCOMPARE(Checksum::compute(data, Checksum::Algorithm::NONE), static_cast<quint32>(0));
        QCOMPARE(Checksum::compute(data, Checksum::Algorithm::CRC32), Checksum::crc32(data));
        QCOMPARE(Checksum::compute(data, Checksum::Algorithm::SUM16), Checksum::simpleSum(data));
    }
};

QTEST_MAIN(TestChecksum)
#include "test_checksum.moc"
