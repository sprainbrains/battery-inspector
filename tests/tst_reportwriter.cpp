#include "metrics.h"
#include "reportwriter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QtTest>

namespace {

QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

SupplySnapshot readFixture(const QString &name)
{
    return SysfsReader(fixturePath(name)).read();
}

QJsonObject supplyObject(const QJsonObject &root, const QString &name)
{
    const QJsonArray supplies = root.value(QStringLiteral("supplies")).toArray();
    for (const QJsonValue &value : supplies) {
        const QJsonObject supply = value.toObject();
        if (supply.value(QStringLiteral("name")).toString() == name)
            return supply;
    }
    return {};
}

QByteArray reportJson(const SupplySnapshot &snapshot, const QVariantMap &summary,
                      const QVariantMap &upower, const QStringList &warnings, bool pretty)
{
    return ReportWriter::toJson(QStringLiteral("1.0.0"), snapshot, summary, upower, warnings,
                                QStringLiteral("2026-11-15T12:00:00Z"), pretty);
}

} // namespace

class TestReportWriter : public QObject
{
    Q_OBJECT

private slots:
    void writesRequiredTopLevelKeys();
    void variantMapMatchesJson();
    void serializesSupplySources();
    void prettyHasNewlines();
    void compactHasNoNewlines();
    void missingUpowerIsEmptyObject();
    void escapesStrings();
    void includesWarnings();
    void skipsNonFiniteNumbers();
};

void TestReportWriter::writesRequiredTopLevelKeys()
{
    const SupplySnapshot snapshot = readFixture(QStringLiteral("amber"));
    const NormalizedMetrics metrics = Metrics::normalize(snapshot);

    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(
            reportJson(snapshot, metrics.values, QVariantMap(),
                       snapshot.warnings + metrics.warnings, true),
            &error);
    QCOMPARE(error.error, QJsonParseError::NoError);
    QVERIFY(document.isObject());
    const QJsonObject root = document.object();

    const QStringList expectedKeys = {
        QStringLiteral("schemaVersion"), QStringLiteral("app"),
        QStringLiteral("appVersion"),    QStringLiteral("timestamp"),
        QStringLiteral("sysfsRoot"),     QStringLiteral("sysfsAvailable"),
        QStringLiteral("supplies"),      QStringLiteral("summary"),
        QStringLiteral("upower"),        QStringLiteral("warnings"),
    };
    for (const QString &key : expectedKeys)
        QVERIFY2(root.contains(key), qPrintable(key));

    QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
    QCOMPARE(root.value(QStringLiteral("app")).toString(), QStringLiteral("Battery Inspector"));
    QCOMPARE(root.value(QStringLiteral("appVersion")).toString(), QStringLiteral("1.0.0"));
    QCOMPARE(root.value(QStringLiteral("timestamp")).toString(),
             QStringLiteral("2026-11-15T12:00:00Z"));
    QCOMPARE(root.value(QStringLiteral("sysfsRoot")).toString(), snapshot.sysfsRoot);
    QCOMPARE(root.value(QStringLiteral("sysfsAvailable")).toBool(), true);
    QVERIFY(root.value(QStringLiteral("supplies")).isArray());
    QCOMPARE(root.value(QStringLiteral("supplies")).toArray().size(), snapshot.supplies.size());
    QVERIFY(root.value(QStringLiteral("summary")).isObject());
    QVERIFY(root.value(QStringLiteral("upower")).isObject());
    QVERIFY(root.value(QStringLiteral("warnings")).isArray());
}

void TestReportWriter::variantMapMatchesJson()
{
    const SupplySnapshot snapshot = readFixture(QStringLiteral("amber"));
    const NormalizedMetrics metrics = Metrics::normalize(snapshot);
    const QVariantMap upower { { QStringLiteral("percentage"), 77.0 } };

    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(
            reportJson(snapshot, metrics.values, upower, snapshot.warnings, true), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);

    const QJsonObject fromMap = QJsonObject::fromVariantMap(ReportWriter::toVariantMap(
            QStringLiteral("1.0.0"), snapshot, metrics.values, upower, snapshot.warnings,
            QStringLiteral("2026-11-15T12:00:00Z")));
    QCOMPARE(fromMap, document.object());
}

void TestReportWriter::serializesSupplySources()
{
    const SupplySnapshot amber = readFixture(QStringLiteral("amber"));
    QJsonParseError error {};
    QJsonDocument document = QJsonDocument::fromJson(
            reportJson(amber, {}, {}, amber.warnings, false), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);

    const QJsonObject battery = supplyObject(document.object(), QStringLiteral("battery"));
    QVERIFY(!battery.isEmpty());
    const QJsonObject status = battery.value(QStringLiteral("attributes"))
                                       .toObject()
                                       .value(QStringLiteral("status"))
                                       .toObject();
    QCOMPARE(status.value(QStringLiteral("value")).toString(), QStringLiteral("Discharging"));
    QCOMPARE(status.value(QStringLiteral("source")).toString(), QStringLiteral("sysfs"));

    const SupplySnapshot ueventOnly = readFixture(QStringLiteral("uevent_only"));
    document = QJsonDocument::fromJson(
            reportJson(ueventOnly, {}, {}, ueventOnly.warnings, false), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);

    const QJsonObject ueventBattery = supplyObject(document.object(), QStringLiteral("battery"));
    QVERIFY(!ueventBattery.isEmpty());
    const QJsonObject capacity = ueventBattery.value(QStringLiteral("attributes"))
                                         .toObject()
                                         .value(QStringLiteral("capacity"))
                                         .toObject();
    QCOMPARE(capacity.value(QStringLiteral("value")).toString(), QStringLiteral("64"));
    QCOMPARE(capacity.value(QStringLiteral("source")).toString(), QStringLiteral("uevent"));
}

void TestReportWriter::prettyHasNewlines()
{
    const SupplySnapshot snapshot = readFixture(QStringLiteral("amber"));
    QVERIFY(reportJson(snapshot, {}, {}, snapshot.warnings, true).contains('\n'));
}

void TestReportWriter::compactHasNoNewlines()
{
    const SupplySnapshot snapshot = readFixture(QStringLiteral("amber"));
    QVERIFY(!reportJson(snapshot, {}, {}, snapshot.warnings, false).contains('\n'));
}

void TestReportWriter::missingUpowerIsEmptyObject()
{
    const SupplySnapshot snapshot = readFixture(QStringLiteral("amber"));

    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(
            reportJson(snapshot, {}, QVariantMap(), snapshot.warnings, true), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);

    const QJsonValue upower = document.object().value(QStringLiteral("upower"));
    QVERIFY(upower.isObject());
    QVERIFY(!upower.isNull());
    QVERIFY(upower.toObject().isEmpty());
}

void TestReportWriter::escapesStrings()
{
    const QString serial = QStringLiteral("SN\"42\nsecond line");

    SupplySnapshot snapshot;
    snapshot.sysfsRoot = QStringLiteral("/sys/class/power_supply");
    snapshot.sysfsAvailable = true;
    snapshot.primaryName = QStringLiteral("battery");

    PowerSupply battery;
    battery.name = QStringLiteral("battery");
    battery.type = QStringLiteral("Battery");
    battery.readable = true;
    battery.attributes.insert(QStringLiteral("serial_number"), serial);
    snapshot.supplies.append(battery);

    const QByteArray json = reportJson(snapshot, {}, {}, {}, false);
    QVERIFY(json.contains(QByteArrayLiteral("\\\"")));
    QVERIFY(json.contains(QByteArrayLiteral("\\n")));

    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    QCOMPARE(error.error, QJsonParseError::NoError);
    const QJsonObject attributes = supplyObject(document.object(), QStringLiteral("battery"))
                                           .value(QStringLiteral("attributes"))
                                           .toObject();
    QCOMPARE(attributes.value(QStringLiteral("serial_number"))
                     .toObject()
                     .value(QStringLiteral("value"))
                     .toString(),
             serial);
}

void TestReportWriter::includesWarnings()
{
    const SupplySnapshot snapshot = readFixture(QStringLiteral("does-not-exist"));
    QVERIFY(!snapshot.sysfsAvailable);
    QVERIFY(snapshot.warnings.contains(QStringLiteral("sysfs-unavailable")));

    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(
            reportJson(snapshot, {}, {}, snapshot.warnings, true), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);

    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("sysfsAvailable")).toBool(), false);
    QCOMPARE(root.value(QStringLiteral("sysfsRoot")).toString(), snapshot.sysfsRoot);

    const QJsonArray warnings = root.value(QStringLiteral("warnings")).toArray();
    QVERIFY(warnings.contains(QJsonValue(QStringLiteral("sysfs-unavailable"))));
}

void TestReportWriter::skipsNonFiniteNumbers()
{
    const SupplySnapshot snapshot = readFixture(QStringLiteral("amber"));
    const QVariantMap summary {
        { QStringLiteral("percentage"), qQNaN() },
        { QStringLiteral("voltage"), qInf() },
        { QStringLiteral("power"), 1.5 },
    };
    const QVariantMap upower {
        { QStringLiteral("energyRate"), -qInf() },
        { QStringLiteral("percentage"), 42.0 },
    };

    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(
            reportJson(snapshot, summary, upower, snapshot.warnings, true), &error);
    QCOMPARE(error.error, QJsonParseError::NoError);

    const QJsonObject summaryObject = document.object().value(QStringLiteral("summary")).toObject();
    QVERIFY(!summaryObject.contains(QStringLiteral("percentage")));
    QVERIFY(!summaryObject.contains(QStringLiteral("voltage")));
    QCOMPARE(summaryObject.value(QStringLiteral("power")).toDouble(), 1.5);

    const QJsonObject upowerObject = document.object().value(QStringLiteral("upower")).toObject();
    QVERIFY(!upowerObject.contains(QStringLiteral("energyRate")));
    QCOMPARE(upowerObject.value(QStringLiteral("percentage")).toDouble(), 42.0);
}

QTEST_GUILESS_MAIN(TestReportWriter)

#include "tst_reportwriter.moc"
