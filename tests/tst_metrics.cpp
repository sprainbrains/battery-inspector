#include "metrics.h"

#include <QtTest>

namespace {

SysfsReader reader(const QString &fixture)
{
    return SysfsReader(QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + fixture);
}

} // namespace

class TestMetrics : public QObject
{
    Q_OBJECT

private slots:
    void normalizesAmberBattery();
    void fallsBackToEnergyWhenChargeMissing();
    void handlesMissingValues();
    void handlesZeroDesignCapacity();
    void clampsPercentage();
    void currentSignAndStatus();
    void timeFromChargeAndCurrent();
    void mergesUpowerOnlyWhenMissing();
    void technologyFromUpowerEnum();
    void upowerOnlyWorks();
    void mapsStatusKeys();
    void upowerStateFillsUnknownStatus();
    void healthAboveDesignCapacity();
    void zeroChargeFullHasNoHealth();
    void zeroChargeNowFallsBackToEnergy();
    void doesNotMirrorSnapshotWarnings();
};

void TestMetrics::normalizesAmberBattery()
{
    const auto m = Metrics::normalize(reader(QStringLiteral("amber")).read());
    QCOMPARE(m.values.value("percentage").toDouble(), 78.0);
    QCOMPARE(m.values.value("statusKey").toString(), QString("discharging"));
    QCOMPARE(m.values.value("voltage").toDouble(), 3.92);
    QVERIFY(qAbs(m.values.value("current").toDouble() - (-0.612)) < 1e-9);
    QVERIFY(qAbs(m.values.value("power").toDouble() - (-2.39904)) < 1e-6);
    QCOMPARE(m.values.value("temperature").toDouble(), 31.8);
    QCOMPARE(m.values.value("healthPercent").toDouble(), 80.0);
    QCOMPARE(m.values.value("wearPercent").toDouble(), 20.0);
    QCOMPARE(m.values.value("cycleCount").toInt(), 214);
    QCOMPARE(m.values.value("chargeFullDesignAh").toDouble(), 4.0);
    QCOMPARE(m.values.value("timeToEmptySec").toInt(), 15120);
    QVERIFY(!m.values.contains("timeToFullSec"));
    QCOMPARE(m.values.value("voltageOcvV").toDouble(), 4.0);
    QVERIFY(qAbs(m.values.value("resistance").toDouble() - 0.12) < 1e-9);
    QCOMPARE(m.values.value("resistanceId").toInt(), 7);
    QCOMPARE(m.values.value("inputCurrentLimitA").toDouble(), 1.5);
}

void TestMetrics::fallsBackToEnergyWhenChargeMissing()
{
    const auto m = Metrics::normalize(reader(QStringLiteral("energy_only")).read());
    QCOMPARE(m.values.value("healthPercent").toDouble(), 80.0);
    QCOMPARE(m.values.value("energyFullWh").toDouble(), 48.0);
    QCOMPARE(m.values.value("energyFullDesignWh").toDouble(), 60.0);
}

void TestMetrics::handlesMissingValues()
{
    const auto m = Metrics::normalize(reader(QStringLiteral("minimal")).read());
    QCOMPARE(m.values.value("percentage").toDouble(), 42.0);
    QVERIFY(!m.values.contains("voltage"));
    QVERIFY(!m.values.contains("healthPercent"));
}

void TestMetrics::handlesZeroDesignCapacity()
{
    const auto m = Metrics::normalize(reader(QStringLiteral("bad_values")).read());
    QVERIFY(!m.values.contains("temperature"));
    QVERIFY(!m.values.contains("percentage"));
    QVERIFY(!m.values.contains("healthPercent"));
    QVERIFY(!m.values.contains("wearPercent"));
}

void TestMetrics::clampsPercentage()
{
    SupplySnapshot s;
    PowerSupply p;
    p.name = "battery"; p.type = "Battery";
    p.attributes["capacity"] = "105";
    s.supplies.append(p); s.primaryName = "battery";
    QCOMPARE(Metrics::normalize(s).values.value("percentage").toDouble(), 100.0);
    s.supplies[0].attributes["capacity"] = "-5";
    QCOMPARE(Metrics::normalize(s).values.value("percentage").toDouble(), 0.0);
}

void TestMetrics::currentSignAndStatus()
{
    SupplySnapshot s;
    PowerSupply p;
    p.name = "battery"; p.type = "Battery";
    p.attributes = {{"status", "Discharging"}, {"current_now", "500000"},
                    {"charge_now", "2000000"}, {"charge_full", "4000000"},
                    {"voltage_now", "4000000"}};
    s.supplies.append(p); s.primaryName = "battery";
    const auto m = Metrics::normalize(s).values;
    QCOMPARE(m.value("statusKey").toString(), QString("discharging"));
    QVERIFY(m.value("timeToEmptySec").toInt() > 0);
    QVERIFY(m.value("power").toDouble() > 0);
}

void TestMetrics::timeFromChargeAndCurrent()
{
    SupplySnapshot s;
    PowerSupply p;
    p.name = "battery"; p.type = "Battery";
    p.attributes = {{"status", "Discharging"}, {"charge_now", "2000000"},
                    {"current_now", "-500000"}};
    s.supplies.append(p); s.primaryName = "battery";
    QCOMPARE(Metrics::normalize(s).values.value("timeToEmptySec").toInt(), 14400);
}

void TestMetrics::mergesUpowerOnlyWhenMissing()
{
    const auto amber = reader(QStringLiteral("amber")).read();
    QVariantMap up{{"voltage", 4.2}, {"chargeCycles", 300}, {"state", 1}};
    const auto merged = Metrics::normalize(amber, up).values;
    QCOMPARE(merged.value("voltage").toDouble(), 3.92);
    QCOMPARE(merged.value("cycleCount").toInt(), 214);
    QCOMPARE(merged.value("statusKey").toString(), QString("discharging"));

    SupplySnapshot noCycles = amber;
    for (auto &supply : noCycles.supplies)
        supply.attributes.remove("cycle_count");
    const auto filled = Metrics::normalize(noCycles, up).values;
    QCOMPARE(filled.value("cycleCount").toInt(), 300);
}

void TestMetrics::technologyFromUpowerEnum()
{
    QVariantMap up{{"technology", 2}};
    QCOMPARE(Metrics::normalize(SupplySnapshot{}, up).values.value("technology").toString(),
             QString("Li-poly"));
}

void TestMetrics::upowerOnlyWorks()
{
    SupplySnapshot empty;
    empty.sysfsAvailable = false;
    QVariantMap up{{"percentage", 55.0}, {"voltage", 4.1},
                   {"capacity", 90.0}, {"chargeCycles", 42}, {"temperature", 30.0},
                   {"energy", 20.0}, {"energyFull", 48.0}, {"energyFullDesign", 60.0},
                   {"energyRate", 7.5}, {"model", "Yandex Phone Battery"},
                   {"vendor", "Yandex"}, {"serial", "SN123456"},
                   {"nativePath", "/org/freedesktop/UPower/devices/battery_BAT0"},
                   {"isPresent", true}, {"state", 2}};
    const auto m = Metrics::normalize(empty, up);
    QCOMPARE(m.values.value("percentage").toDouble(), 55.0);
    QCOMPARE(m.values.value("healthPercent").toDouble(), 90.0);
    QCOMPARE(m.values.value("cycleCount").toInt(), 42);
    QCOMPARE(m.values.value("voltage").toDouble(), 4.1);
    QCOMPARE(m.values.value("temperature").toDouble(), 30.0);
    QCOMPARE(m.values.value("energyNowWh").toDouble(), 20.0);
    QCOMPARE(m.values.value("energyFullWh").toDouble(), 48.0);
    QCOMPARE(m.values.value("energyFullDesignWh").toDouble(), 60.0);
    QCOMPARE(m.values.value("power").toDouble(), 7.5);
    QCOMPARE(m.values.value("modelName").toString(), QString("Yandex Phone Battery"));
    QCOMPARE(m.values.value("manufacturer").toString(), QString("Yandex"));
    QCOMPARE(m.values.value("serialNumber").toString(), QString("SN123456"));
    QCOMPARE(m.values.value("nativePath").toString(),
             QString("/org/freedesktop/UPower/devices/battery_BAT0"));
    QCOMPARE(m.values.value("present").toBool(), true);
    QCOMPARE(m.values.value("statusKey").toString(), QString("discharging"));
}

void TestMetrics::mapsStatusKeys()
{
    const auto keyFor = [](const QString &status) {
        SupplySnapshot s;
        PowerSupply p;
        p.name = "battery"; p.type = "Battery";
        p.attributes["status"] = status;
        s.supplies.append(p); s.primaryName = "battery";
        return Metrics::normalize(s).values.value("statusKey").toString();
    };
    QCOMPARE(keyFor(QStringLiteral("Charging")), QString("charging"));
    QCOMPARE(keyFor(QStringLiteral("Discharging")), QString("discharging"));
    QCOMPARE(keyFor(QStringLiteral("Full")), QString("full"));
    QCOMPARE(keyFor(QStringLiteral("Not charging")), QString("notCharging"));
    QCOMPARE(keyFor(QStringLiteral("Unknown")), QString("unknown"));
}

void TestMetrics::upowerStateFillsUnknownStatus()
{
    SupplySnapshot s;
    PowerSupply p;
    p.name = "battery"; p.type = "Battery";
    p.attributes["status"] = "Unknown";
    s.supplies.append(p); s.primaryName = "battery";
    QVariantMap up{{"state", 5}};
    QCOMPARE(Metrics::normalize(s, up).values.value("statusKey").toString(),
             QString("charging"));
}

void TestMetrics::healthAboveDesignCapacity()
{
    SupplySnapshot s;
    PowerSupply p;
    p.name = "battery"; p.type = "Battery";
    p.attributes = {{"charge_full", "4500000"}, {"charge_full_design", "4000000"}};
    s.supplies.append(p); s.primaryName = "battery";
    const auto m = Metrics::normalize(s).values;
    QCOMPARE(m.value("healthPercent").toDouble(), 112.5);
    QCOMPARE(m.value("wearPercent").toDouble(), 0.0);
}

void TestMetrics::zeroChargeFullHasNoHealth()
{
    SupplySnapshot s;
    PowerSupply p;
    p.name = "battery"; p.type = "Battery";
    p.attributes = {{"charge_now", "2000000"}, {"charge_full", "0"},
                    {"charge_full_design", "4000000"}};
    s.supplies.append(p); s.primaryName = "battery";
    const auto m = Metrics::normalize(s).values;
    QVERIFY(!m.contains("healthPercent"));
    QVERIFY(!m.contains("wearPercent"));
}

void TestMetrics::zeroChargeNowFallsBackToEnergy()
{
    SupplySnapshot s;
    PowerSupply p;
    p.name = "battery"; p.type = "Battery";
    p.attributes = {{"status", "Discharging"}, {"charge_now", "0"},
                    {"charge_full", "4000000"},
                    {"energy_now", "30000000"}, {"energy_full", "60000000"},
                    {"power_now", "5000000"}};
    s.supplies.append(p); s.primaryName = "battery";
    const auto m = Metrics::normalize(s).values;
    QCOMPARE(m.value("energyNowWh").toDouble(), 30.0);
    QCOMPARE(m.value("timeToEmptySec").toInt(), 21600);
}

void TestMetrics::doesNotMirrorSnapshotWarnings()
{
    SupplySnapshot s;
    s.warnings << QStringLiteral("no-battery");
    QVERIFY(Metrics::normalize(s).warnings.isEmpty());
}

QTEST_GUILESS_MAIN(TestMetrics)

#include "tst_metrics.moc"
