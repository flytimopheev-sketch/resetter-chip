#include <QTest>

#include "core/ChipProfile.h"
#include "core/ProfileManager.h"

#include <QDir>

class TestProfile : public QObject
{
    Q_OBJECT

private slots:
    void fromJsonParsesGeometry()
    {
        QJsonObject obj;
        obj[QStringLiteral("id")] = QStringLiteral("24C16");
        obj[QStringLiteral("name")] = QStringLiteral("Test Chip");
        obj[QStringLiteral("size_bytes")] = 2048;
        obj[QStringLiteral("i2c_address")] = 0x50;
        obj[QStringLiteral("two_byte_address")] = false;
        obj[QStringLiteral("block_bits")] = 3;
        obj[QStringLiteral("page_size")] = 16;
        obj[QStringLiteral("write_cycle_ms")] = 5;
        obj[QStringLiteral("family")] = QStringLiteral("24Cxx");
        obj[QStringLiteral("counter_offset")] = 255;
        obj[QStringLiteral("counter_size")] = 2;

        const ChipProfile p = ChipProfile::fromJson(obj);
        QCOMPARE(p.id, QStringLiteral("24C16"));
        QCOMPARE(p.sizeBytes, static_cast<quint32>(2048));
        QCOMPARE(p.i2cAddress, static_cast<quint8>(0x50));
        QCOMPARE(p.blockBits, static_cast<quint8>(3));
        QCOMPARE(p.pageSize, static_cast<quint16>(16));
        QCOMPARE(p.counterOffset, static_cast<quint32>(255));
        QCOMPARE(p.counterSize, static_cast<quint32>(2));
    }

    void toJsonRoundTrip()
    {
        ChipProfile p;
        p.id = QStringLiteral("24C02");
        p.name = QStringLiteral("24C02 Test");
        p.sizeBytes = 256;
        p.pageSize = 8;
        p.family = QStringLiteral("24Cxx");
        p.protectedRanges.append(qMakePair<quint32, quint32>(0, 16));

        const QJsonObject obj = p.toJson();
        QCOMPARE(obj.value(QStringLiteral("id")).toString(), QStringLiteral("24C02"));
        QCOMPARE(obj.value(QStringLiteral("size_bytes")).toInt(), 256);

        const ChipProfile restored = ChipProfile::fromJson(obj);
        QCOMPARE(restored.id, p.id);
        QCOMPARE(restored.sizeBytes, p.sizeBytes);
        QCOMPARE(restored.protectedRanges.size(), 1);
    }

    void protectedRangeCheck()
    {
        ChipProfile p;
        p.id = QStringLiteral("TEST");
        p.sizeBytes = 256;
        p.protectedRanges.append(qMakePair<quint32, quint32>(0x00, 0x10));

        QVERIFY(!p.isAddressProtected(0x20, 1));
        QVERIFY(p.isAddressProtected(0x08, 1));
        QVERIFY(p.isAddressProtected(0x0F, 1));
        QVERIFY(!p.isAddressProtected(0x10, 1));
        QVERIFY(p.isAddressProtected(0x0F, 4));
    }

    void loadsBundledProfiles()
    {
        const QString dir = QStringLiteral(RC_PROFILES_DIR);
        QVERIFY2(QDir(dir).exists(), "Каталог профилей недоступен");

        ProfileManager &pm = ProfileManager::instance();
        QVERIFY(pm.loadProfiles(dir));

        QVERIFY(pm.profile(QStringLiteral("24C16")) != nullptr);
        QVERIFY(pm.profile(QStringLiteral("24C256")) != nullptr);
        QVERIFY(pm.profile(QStringLiteral("24C1024")) != nullptr);

        const auto chip = pm.profile(QStringLiteral("24C16"));
        QCOMPARE(chip->sizeBytes, static_cast<quint32>(2048));
        QCOMPARE(chip->pageSize, static_cast<quint16>(16));
        QCOMPARE(chip->counterOffset, static_cast<quint32>(0xFFFFFFFFu));
        QCOMPARE(chip->protectedRanges.size(), 2);
        QCOMPARE(chip->protectedRanges.at(1).first, static_cast<quint32>(2032));
        QCOMPARE(chip->protectedRanges.at(1).second, static_cast<quint32>(2048));

        const auto large = pm.profile(QStringLiteral("24C1024"));
        QVERIFY(large != nullptr);
        QCOMPARE(large->twoByteAddress, true);
        QCOMPARE(large->blockBits, static_cast<quint8>(1));

        QCOMPARE(pm.cartridgeCount(), 2);
        const auto resolved = pm.resolveProfile(QStringLiteral("HP_CC364A"));
        QVERIFY2(resolved != nullptr, "Профиль картриджа HP должен разрешаться в 24C16");
        QCOMPARE(resolved->sizeBytes, static_cast<quint32>(2048));
        QCOMPARE(resolved->counterOffset, static_cast<quint32>(255));
        QCOMPARE(resolved->counterSize, static_cast<quint32>(2));
        QCOMPARE(resolved->allowedRanges.size(), 1);

        // Счётчик не должен выходить за границу 1024-байтного 24C08.
        const auto samsung = pm.resolveProfile(QStringLiteral("Samsung_MLT_D101S"));
        QVERIFY2(samsung != nullptr, "Профиль картриджа Samsung должен разрешаться в 24C08");
        QCOMPARE(samsung->counterOffset, static_cast<quint32>(1022));
        QCOMPARE(samsung->counterSize, static_cast<quint32>(2));
    }
};

QTEST_MAIN(TestProfile)
#include "test_profile.moc"
