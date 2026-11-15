#include "upowersource.h"

#include <QDBusConnection>
#include <QtTest>

class TestUpower : public QObject
{
    Q_OBJECT

private slots:
    void mapsHyphenatedKeys();
    void mapsPascalCaseKeys();
    void filtersBatteryDevices();
    void mapsTechnologyEnum();
    void dropsNegativeCycles();
    void mapsThresholds();
    void keepsUnknownKeys();
    void blockingReadWithoutSystemBusReturnsEmpty();
};

void TestUpower::mapsHyphenatedKeys()
{
    const QVariantMap mapped = UpowerSource::mapProperties({{"charge-cycles", 214},
                                                           {"energy-full", 12.5},
                                                           {"time-to-empty", 3600}});
    QCOMPARE(mapped.value("chargeCycles").toInt(), 214);
    QCOMPARE(mapped.value("energyFull").toDouble(), 12.5);
    QCOMPARE(mapped.value("timeToEmpty").toInt(), 3600);
}

void TestUpower::mapsPascalCaseKeys()
{
    const QVariantMap mapped = UpowerSource::mapProperties({{"ChargeCycles", 214},
                                                           {"EnergyFull", 12.5}});
    QCOMPARE(mapped.value("chargeCycles").toInt(), 214);
    QCOMPARE(mapped.value("energyFull").toDouble(), 12.5);
}

void TestUpower::filtersBatteryDevices()
{
    QVERIFY(UpowerSource::isBatteryDevice({{"type", 2}}));
    QVERIFY(!UpowerSource::isBatteryDevice({{"type", 1}}));
    QVERIFY(UpowerSource::isBatteryDevice({{"type", 2}, {"power-supply", false}}));
}

void TestUpower::mapsTechnologyEnum()
{
    const QVariantMap mapped = UpowerSource::mapProperties({{"technology", 2}});
    QCOMPARE(mapped.value("technology").toString(), QString("Li-poly"));
}

void TestUpower::dropsNegativeCycles()
{
    const QVariantMap mapped = UpowerSource::mapProperties({{"charge-cycles", -1}});
    QVERIFY(!mapped.contains("chargeCycles"));
}

void TestUpower::mapsThresholds()
{
    const QVariantMap mapped = UpowerSource::mapProperties({{"charge-start-threshold", 20},
                                                           {"charge-end-threshold", 80}});
    QCOMPARE(mapped.value("chargeStartThreshold").toInt(), 20);
    QCOMPARE(mapped.value("chargeEndThreshold").toInt(), 80);
}

void TestUpower::keepsUnknownKeys()
{
    const QVariantMap mapped = UpowerSource::mapProperties({{"some-new-property", 5}});
    QCOMPARE(mapped.value("someNewProperty").toInt(), 5);
}

void TestUpower::blockingReadWithoutSystemBusReturnsEmpty()
{
    if (QDBusConnection::systemBus().isConnected())
        QSKIP("system bus is available");
    QVERIFY(RealUpowerSource().blockingRead(100).isEmpty());
}

QTEST_GUILESS_MAIN(TestUpower)

#include "tst_upower.moc"
