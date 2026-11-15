#include <QTest>

#include "core/ChipProfile.h"
#include "core/CounterReset.h"

class TestCounter : public QObject
{
    Q_OBJECT

private slots:
    void littleEndianRoundTrip()
    {
        ChipProfile p;
        p.sizeBytes = 256; p.counterOffset = 16; p.counterSize = 2;
        QByteArray data(256, '\0');
        QVERIFY(CounterReset::setCounter(data, p, 0x1234));
        QCOMPARE(static_cast<quint8>(data.at(16)), static_cast<quint8>(0x34));
        QCOMPARE(static_cast<quint8>(data.at(17)), static_cast<quint8>(0x12));
        QCOMPARE(CounterReset::readCounter(data, p).value(), 0x1234u);
    }

    void bigEndianRoundTrip()
    {
        ChipProfile p;
        p.sizeBytes = 256; p.counterOffset = 16; p.counterSize = 2; p.counterBigEndian = true;
        QByteArray data(256, '\0');
        QVERIFY(CounterReset::setCounter(data, p, 0x1234));
        QCOMPARE(static_cast<quint8>(data.at(16)), static_cast<quint8>(0x12));
        QCOMPARE(static_cast<quint8>(data.at(17)), static_cast<quint8>(0x34));
        QCOMPARE(CounterReset::readCounter(data, p).value(), 0x1234u);
    }

    void resetUsesInitialValue()
    {
        ChipProfile p;
        p.sizeBytes = 256; p.counterOffset = 0; p.counterSize = 2; p.counterInitialValue = 0x00FF;
        QByteArray data(256, '\xAA');
        const auto out = CounterReset::resetCounter(data, p);
        QVERIFY(out.has_value());
        QCOMPARE(CounterReset::readCounter(*out, p).value(), 0x00FFu);
    }

    void resetDefaultsToZero()
    {
        ChipProfile p;
        p.sizeBytes = 256; p.counterOffset = 0; p.counterSize = 4;
        QByteArray data(256, '\xAA');
        const auto out = CounterReset::resetCounter(data, p);
        QVERIFY(out.has_value());
        QCOMPARE(CounterReset::readCounter(*out, p).value(), 0u);
    }

    void outOfBoundsRejected()
    {
        ChipProfile p;
        p.sizeBytes = 16; p.counterOffset = 15; p.counterSize = 2;
        QByteArray data(16, '\0');
        QVERIFY(!CounterReset::readCounter(data, p).has_value());
        QVERIFY(!CounterReset::setCounter(data, p, 1));
    }

    void unsetOffsetRejected()
    {
        ChipProfile p;
        p.sizeBytes = 256; p.counterOffset = 0xFFFFFFFFu; p.counterSize = 2;
        QByteArray data(256, '\0');
        QVERIFY(!CounterReset::readCounter(data, p).has_value());
    }
};

QTEST_MAIN(TestCounter)
#include "test_counter.moc"
