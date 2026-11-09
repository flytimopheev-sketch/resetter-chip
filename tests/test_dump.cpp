#include <QCoreApplication>
#include <QTest>
#include <QByteArray>
#include <QTemporaryDir>
#include <QFile>
#include "../src/core/DumpManager.h"
#include "../src/core/Checksum.h"

#include <cstring>

class TestDump : public QObject
{
    Q_OBJECT
private slots:
    void testCompareDumpsIdentical()
    {
        QByteArray a("\x00\x01\x02\x03\x04", 5);
        QByteArray b("\x00\x01\x02\x03\x04", 5);
        QVector<quint32> diff = DumpManager::compareDumps(a, b);
        QCOMPARE(diff.size(), 0);
    }

    void testCompareDumpsDifferent()
    {
        QByteArray a("\x00\x01\x02\x03\x04", 5);
        QByteArray b("\x00\xFF\x02\x03\x04", 5);
        QVector<quint32> diff = DumpManager::compareDumps(a, b);
        QCOMPARE(diff.size(), 1);
        QCOMPARE(diff[0], static_cast<quint32>(1));
    }

    void testCompareDumpsDifferentSizes()
    {
        QByteArray a("\x00\x01\x02", 3);
        QByteArray b("\x00\x01\x02\x03\x04", 5);
        QVector<quint32> diff = DumpManager::compareDumps(a, b);
        QCOMPARE(diff.size(), 2);
    }

    void testCountDifferences()
    {
        QByteArray a("\x00\x01\x02\x03", 4);
        QByteArray b("\x00\xFF\x02\xFF", 4);
        QCOMPARE(DumpManager::countDifferences(a, b), 2);
    }

    void testSaveAndLoadDump()
    {
        QTemporaryDir tmpDir;
        QString filePath = tmpDir.path() + "/test_dump.bin";
        QByteArray data("\xAA\xBB\xCC\xDD", 4);

        bool saved = DumpManager::saveDump(filePath, data, "24C16", "TestCartridge");
        QVERIFY(saved);

        auto loaded = DumpManager::loadDump(filePath);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->size(), 4);
        QCOMPARE(memcmp(loaded->constData(), data.constData(), 4), 0);
    }

    void testDumpInfo()
    {
        QTemporaryDir tmpDir;
        QString filePath = tmpDir.path() + "/test_info.bin";
        QByteArray data("\x00\x01\x02\x03\x04\x05", 6);
        DumpManager::saveDump(filePath, data, "24C02", "Samsung");

        auto info = DumpManager::dumpInfo(filePath);
        QVERIFY(info.has_value());
        QCOMPARE(info->chipId, QString("24C02"));
        QCOMPARE(info->cartridgeModel, QString("Samsung"));
    }
};

QTEST_MAIN(TestDump)
#include "test_dump.moc"
