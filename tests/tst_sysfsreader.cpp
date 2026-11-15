#include "sysfsreader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

const PowerSupply *findSupply(const SupplySnapshot &snapshot, const QString &name)
{
    for (const PowerSupply &supply : snapshot.supplies) {
        if (supply.name == name)
            return &supply;
    }
    return nullptr;
}

QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

} // namespace

class TestSysfsReader : public QObject
{
    Q_OBJECT

private slots:
    void readsAmberBatteryAndBms();
    void parsesUevent();
    void ueventOnlyAttributes();
    void missingRoot();
    void emptyRoot();
    void ignoresBinaryAndUnreadable();
    void selectsBmsCompanion();
    void followsSymlinkedSupplyDirectory();
    void defaultRootHonoursEnvironment();
};

void TestSysfsReader::readsAmberBatteryAndBms()
{
    const SysfsReader reader(fixturePath(QStringLiteral("amber")));
    const SupplySnapshot snapshot = reader.read();

    QVERIFY(snapshot.sysfsAvailable);
    QCOMPARE(snapshot.supplies.size(), 3);
    QCOMPARE(snapshot.primaryName, QStringLiteral("battery"));
    QCOMPARE(snapshot.bmsName, QStringLiteral("bms"));

    const PowerSupply *battery = findSupply(snapshot, QStringLiteral("battery"));
    QVERIFY(battery);
    QCOMPARE(battery->type, QStringLiteral("Battery"));
    QCOMPARE(battery->attributes.value(QStringLiteral("status")), QStringLiteral("Discharging"));
    QCOMPARE(battery->attributes.value(QStringLiteral("capacity")), QStringLiteral("78"));

    const PowerSupply *bms = findSupply(snapshot, QStringLiteral("bms"));
    QVERIFY(bms);
    QCOMPARE(bms->attributes.value(QStringLiteral("charge_full_design")),
             QStringLiteral("4000000"));
    QCOMPARE(bms->attributes.value(QStringLiteral("time_to_empty_avg")), QStringLiteral("15120"));
}

void TestSysfsReader::parsesUevent()
{
    const QMap<QString, QString> values = SysfsReader::parseUevent(
            QStringLiteral("POWER_SUPPLY_STATUS=Charging\nPOWER_SUPPLY_CAPACITY=64\n"));

    QCOMPARE(values.value(QStringLiteral("status")), QStringLiteral("Charging"));
    QCOMPARE(values.value(QStringLiteral("capacity")), QStringLiteral("64"));
}

void TestSysfsReader::ueventOnlyAttributes()
{
    const SysfsReader reader(fixturePath(QStringLiteral("uevent_only")));
    const SupplySnapshot snapshot = reader.read();

    const PowerSupply *battery = findSupply(snapshot, QStringLiteral("battery"));
    QVERIFY(battery);
    QCOMPARE(battery->attributes.value(QStringLiteral("capacity")), QStringLiteral("64"));
    QCOMPARE(battery->attributes.value(QStringLiteral("voltage_now")), QStringLiteral("4100000"));
    QVERIFY(battery->fromUevent.contains(QStringLiteral("capacity")));
    QVERIFY(battery->fromUevent.contains(QStringLiteral("voltage_now")));
}

void TestSysfsReader::missingRoot()
{
    const SysfsReader reader(fixturePath(QStringLiteral("does-not-exist")));
    const SupplySnapshot snapshot = reader.read();

    QVERIFY(!snapshot.sysfsAvailable);
    QVERIFY(snapshot.warnings.contains(QStringLiteral("sysfs-unavailable")));
}

void TestSysfsReader::emptyRoot()
{
    QTemporaryDir empty;
    QVERIFY(empty.isValid());

    const SysfsReader reader(empty.path());
    const SupplySnapshot snapshot = reader.read();

    QVERIFY(snapshot.sysfsAvailable);
    QVERIFY(snapshot.primaryName.isEmpty());
    QVERIFY(snapshot.warnings.contains(QStringLiteral("no-battery")));
}

void TestSysfsReader::ignoresBinaryAndUnreadable()
{
    const SysfsReader reader(fixturePath(QStringLiteral("bad_values")));
    const SupplySnapshot snapshot = reader.read();

    const PowerSupply *battery = findSupply(snapshot, QStringLiteral("battery"));
    QVERIFY(battery);
    QVERIFY(!battery->attributes.contains(QStringLiteral("junk")));
    QCOMPARE(battery->attributes.value(QStringLiteral("capacity")), QStringLiteral("abc"));
}

void TestSysfsReader::selectsBmsCompanion()
{
    const SysfsReader reader(fixturePath(QStringLiteral("multi")));
    const SupplySnapshot snapshot = reader.read();

    QCOMPARE(snapshot.primaryName, QStringLiteral("battery"));
    QCOMPARE(snapshot.bmsName, QStringLiteral("bms"));
    QVERIFY(findSupply(snapshot, QStringLiteral("usb")));
    QVERIFY(findSupply(snapshot, QStringLiteral("ac")));
}

void TestSysfsReader::followsSymlinkedSupplyDirectory()
{
    QTemporaryDir target;
    QVERIFY(target.isValid());
    QFile typeFile(target.path() + QStringLiteral("/type"));
    QVERIFY(typeFile.open(QIODevice::WriteOnly));
    QCOMPARE(typeFile.write("Battery\n"), qint64(8));
    typeFile.close();

    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QFile::link(target.path(), root.path() + QStringLiteral("/battery")));

    const SysfsReader reader(root.path());
    const SupplySnapshot snapshot = reader.read();

    QCOMPARE(snapshot.supplies.size(), 1);
    QCOMPARE(snapshot.supplies.first().name, QStringLiteral("battery"));
    QCOMPARE(snapshot.primaryName, QStringLiteral("battery"));
    QCOMPARE(snapshot.supplies.first().type, QStringLiteral("Battery"));
}

void TestSysfsReader::defaultRootHonoursEnvironment()
{
    const QByteArray previous = qgetenv("BATTERY_SYSFS_ROOT");
    const QString customRoot = QStringLiteral("/tmp/battery-inspector-sysfs");

    qputenv("BATTERY_SYSFS_ROOT", customRoot.toUtf8());
    QCOMPARE(SysfsReader::defaultRoot(), customRoot);
    QCOMPARE(SysfsReader().root(), customRoot);

    qunsetenv("BATTERY_SYSFS_ROOT");
    QCOMPARE(SysfsReader::defaultRoot(), QStringLiteral("/sys/class/power_supply"));

    if (!previous.isEmpty())
        qputenv("BATTERY_SYSFS_ROOT", previous);
}

QTEST_GUILESS_MAIN(TestSysfsReader)

#include "tst_sysfsreader.moc"
