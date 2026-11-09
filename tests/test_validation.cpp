#include <QTest>

#include "core/ChipProfile.h"
#include "core/CounterReset.h"
#include "core/Validator.h"

class TestValidation : public QObject
{
    Q_OBJECT

private:
    static ChipProfile makeProfile()
    {
        ChipProfile p;
        p.id = QStringLiteral("24C16");
        p.sizeBytes = 2048;
        p.pageSize = 16;
        p.counterOffset = 0xFF;
        p.counterSize = 2;
        p.protectedRanges.append(qMakePair<quint32, quint32>(0x000, 0x010));
        return p;
    }

private slots:
    void sizeValidation()
    {
        const ChipProfile profile = makeProfile();
        const QByteArray good(2048, '\0');
        const QByteArray bad(1024, '\0');

        QVERIFY(Validator::validateSize(good, profile));

        QString error;
        QVERIFY(!Validator::validateSize(bad, profile, &error));
        QVERIFY(!error.isEmpty());
    }

    void protectedRangeBlocksChanges()
    {
        const ChipProfile profile = makeProfile();

        QByteArray original(2048, '\0');
        QByteArray modified = original;
        modified[0x10] = '\x42';

        QVERIFY(Validator::validateProtectedRanges(original, modified, profile));

        QByteArray forbidden = original;
        forbidden[0x00] = '\x42';

        QString error;
        QVERIFY(!Validator::validateProtectedRanges(original, forbidden, profile, &error));
        QVERIFY(error.contains(QStringLiteral("0x0000")));
    }

    void counterReadWrite()
    {
        const ChipProfile profile = makeProfile();
        QByteArray data(2048, '\0');

        data[0xFF] = '\x34';
        data[0x100] = '\x12';

        const auto value = CounterReset::readCounter(data, profile);
        QVERIFY(value.has_value());

        QVERIFY(CounterReset::setCounter(data, profile, 0));
        QCOMPARE(data.at(0xFF), '\0');
        QCOMPARE(data.at(0x100), '\0');

        const auto reset = CounterReset::resetCounter(data, profile);
        QVERIFY(reset.has_value());
        QCOMPARE(reset->size(), data.size());
    }

    void counterUnavailableWhenNotConfigured()
    {
        ChipProfile profile = makeProfile();
        profile.counterOffset = 0xFFFFFFFF;

        QByteArray data(2048, '\0');
        QVERIFY(!CounterReset::readCounter(data, profile).has_value());
        QVERIFY(!CounterReset::resetCounter(data, profile).has_value());
    }
};

QTEST_MAIN(TestValidation)
#include "test_validation.moc"
