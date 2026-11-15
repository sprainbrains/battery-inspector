#include "batteryprovider.h"

#include "upowersource.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class FakeUpowerSource : public UpowerSource
{
    Q_OBJECT
public:
    explicit FakeUpowerSource(QObject *parent = nullptr)
        : UpowerSource(parent)
    {
    }

    void refresh() override
    {
        if (device.isEmpty())
            emit unavailable(QStringLiteral("no-battery"));
        else
            emit updated(device);
    }

    QVariantMap blockingRead(int) const override { return device; }

    QVariantMap device;
};

class TestBatteryProvider : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void readsAmberFixture();
    void fillsFromUpower();
    void upowerOnlyWhenSysfsUnavailable();
    void reportsNoBattery();
    void reportsSysfsUnavailable();
    void clampsPollInterval();
    void savesReport();
    void historyGrowsOnRefresh();
    void rawAttributesIncludeUeventAndUpower();
    void powerSuppliesList();
    void reportJsonIsValid();

private:
    static QString fixture(const QString &name);
};

QString TestBatteryProvider::fixture(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

void TestBatteryProvider::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestBatteryProvider::cleanup()
{
    qunsetenv("BATTERY_SYSFS_ROOT");
}

void TestBatteryProvider::readsAmberFixture()
{
    qputenv("BATTERY_SYSFS_ROOT", fixture("amber").toUtf8());
    FakeUpowerSource fake;
    BatteryProvider provider(&fake);
    provider.refresh();

    QCOMPARE(provider.summary().value("voltage").toDouble(), 3.92);
    QCOMPARE(provider.summary().value("healthPercent").toDouble(), 80.0);
    QCOMPARE(provider.summary().value("cycleCount").toInt(), 214);
    QCOMPARE(provider.sysfsStatus(), QStringLiteral("ok"));
}

void TestBatteryProvider::fillsFromUpower()
{
    qputenv("BATTERY_SYSFS_ROOT", fixture("minimal").toUtf8());
    FakeUpowerSource fake;
    fake.device = {{QStringLiteral("voltage"), 4.2},
                   {QStringLiteral("chargeCycles"), 300},
                   {QStringLiteral("capacity"), 91.0}};
    BatteryProvider provider(&fake);
    provider.refresh();

    QCOMPARE(provider.summary().value("voltage").toDouble(), 4.2);
    QCOMPARE(provider.summary().value("cycleCount").toInt(), 300);
    QCOMPARE(provider.upowerStatus(), QStringLiteral("ok"));

    qputenv("BATTERY_SYSFS_ROOT", fixture("amber").toUtf8());
    BatteryProvider sysfsWins(&fake);
    sysfsWins.refresh();
    QCOMPARE(sysfsWins.summary().value("voltage").toDouble(), 3.92);
    QCOMPARE(sysfsWins.summary().value("cycleCount").toInt(), 214);
}

void TestBatteryProvider::upowerOnlyWhenSysfsUnavailable()
{
    qputenv("BATTERY_SYSFS_ROOT", "/nonexistent/battery-inspector-test");
    FakeUpowerSource fake;
    fake.device = {{QStringLiteral("percentage"), 55.0},
                   {QStringLiteral("voltage"), 4.1},
                   {QStringLiteral("temperature"), 30.0}};
    BatteryProvider provider(&fake);
    provider.refresh();

    QCOMPARE(provider.sysfsStatus(), QStringLiteral("unavailable"));
    QCOMPARE(provider.upowerStatus(), QStringLiteral("ok"));
    QCOMPARE(provider.summary().value("percentage").toDouble(), 55.0);
    QCOMPARE(provider.summary().value("voltage").toDouble(), 4.1);
}

void TestBatteryProvider::reportsNoBattery()
{
    QTemporaryDir emptyDir;
    QVERIFY(emptyDir.isValid());
    qputenv("BATTERY_SYSFS_ROOT", emptyDir.path().toUtf8());
    FakeUpowerSource fake;
    BatteryProvider provider(&fake);
    provider.refresh();

    QCOMPARE(provider.sysfsStatus(), QStringLiteral("no-battery"));
}

void TestBatteryProvider::reportsSysfsUnavailable()
{
    qputenv("BATTERY_SYSFS_ROOT", "/nonexistent/battery-inspector-test");
    FakeUpowerSource fake;
    BatteryProvider provider(&fake);
    provider.refresh();

    QCOMPARE(provider.sysfsStatus(), QStringLiteral("unavailable"));
}

void TestBatteryProvider::clampsPollInterval()
{
    FakeUpowerSource fake;
    BatteryProvider provider(&fake);

    provider.setPollInterval(0);
    QCOMPARE(provider.pollInterval(), 1000);
    provider.setPollInterval(60000);
    QCOMPARE(provider.pollInterval(), 10000);
    provider.setPollInterval(2000);
    QCOMPARE(provider.pollInterval(), 2000);
}

void TestBatteryProvider::savesReport()
{
    qputenv("BATTERY_SYSFS_ROOT", fixture("amber").toUtf8());
    FakeUpowerSource fake;
    fake.device = {{QStringLiteral("voltage"), 4.2}};
    BatteryProvider provider(&fake);
    provider.refresh();

    const QString path = provider.saveReport();
    QVERIFY2(!path.isEmpty(), "saveReport returned an empty path");
    QVERIFY(QFile::exists(path));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    QVERIFY(document.isObject());
    QVERIFY(document.object().contains(QStringLiteral("supplies")));
    file.close();
    QFile::remove(path);
}

void TestBatteryProvider::historyGrowsOnRefresh()
{
    qputenv("BATTERY_SYSFS_ROOT", fixture("amber").toUtf8());
    FakeUpowerSource fake;
    BatteryProvider provider(&fake);
    provider.refresh();
    const int first = provider.history().size();
    provider.refresh();
    QVERIFY(provider.history().size() >= first + 1);
}

void TestBatteryProvider::rawAttributesIncludeUeventAndUpower()
{
    qputenv("BATTERY_SYSFS_ROOT", fixture("amber").toUtf8());
    FakeUpowerSource fake;
    fake.device = {{QStringLiteral("voltage"), 4.2}};
    BatteryProvider provider(&fake);
    provider.refresh();

    bool statusRow = false;
    bool upowerRow = false;
    const QVariantList rows = provider.rawAttributes();
    for (const QVariant &entry : rows) {
        const QVariantMap row = entry.toMap();
        if (row.value("supply").toString() == QLatin1String("battery")
            && row.value("name").toString() == QLatin1String("status")
            && row.value("source").toString() == QLatin1String("sysfs")) {
            statusRow = true;
        }
        if (row.value("supply").toString() == QLatin1String("upower")
            && row.value("name").toString() == QLatin1String("voltage")
            && row.value("source").toString() == QLatin1String("upower")) {
            upowerRow = true;
        }
    }
    QVERIFY2(statusRow, "battery/status sysfs row not found");
    QVERIFY2(upowerRow, "upower voltage row not found");
}

void TestBatteryProvider::powerSuppliesList()
{
    qputenv("BATTERY_SYSFS_ROOT", fixture("amber").toUtf8());
    FakeUpowerSource fake;
    BatteryProvider provider(&fake);
    provider.refresh();

    bool usbOnline = false;
    const QVariantList supplies = provider.supplies();
    for (const QVariant &entry : supplies) {
        const QVariantMap supply = entry.toMap();
        if (supply.value("name").toString() == QLatin1String("usb")
            && supply.value("online").toBool()) {
            usbOnline = true;
        }
    }
    QVERIFY2(usbOnline, "usb supply with online=true not found");
}

void TestBatteryProvider::reportJsonIsValid()
{
    qputenv("BATTERY_SYSFS_ROOT", fixture("amber").toUtf8());
    FakeUpowerSource fake;
    BatteryProvider provider(&fake);
    provider.refresh();

    const QJsonDocument document = QJsonDocument::fromJson(provider.reportJson().toUtf8());
    QVERIFY(document.isObject());
    QVERIFY(document.object().contains(QStringLiteral("summary")));
}

QTEST_GUILESS_MAIN(TestBatteryProvider)

#include "tst_batteryprovider.moc"
