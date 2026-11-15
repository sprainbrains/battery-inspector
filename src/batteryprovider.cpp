#include "batteryprovider.h"

#include "metrics.h"
#include "reportwriter.h"
#include "upowersource.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

constexpr int MinPollIntervalMs = 1000;
constexpr int MaxPollIntervalMs = 10000;
const char *const AppVersion = "1.0.0";
const char *const ReportFileName = "report.json";

} // namespace

BatteryProvider::BatteryProvider(UpowerSource *upower, QObject *parent)
    : QObject(parent)
    , m_upower(upower ? upower : new RealUpowerSource(this))
{
    connect(m_upower, &UpowerSource::updated, this, &BatteryProvider::onUpowerUpdated);
    connect(m_upower, &UpowerSource::unavailable, this, &BatteryProvider::onUpowerUnavailable);
    connect(&m_timer, &QTimer::timeout, this, &BatteryProvider::onTick);
    m_timer.setInterval(m_pollIntervalMs);
}

BatteryProvider::~BatteryProvider() = default;

QVariantMap BatteryProvider::summary() const
{
    return m_summary;
}

QVariantMap BatteryProvider::upower() const
{
    return m_upowerData;
}

QVariantList BatteryProvider::rawAttributes() const
{
    return buildRawAttributes();
}

QVariantList BatteryProvider::supplies() const
{
    return buildSupplies();
}

QVariantList BatteryProvider::history() const
{
    return m_history.toVariantList();
}

QVariantMap BatteryProvider::historyStats() const
{
    QVariantMap result;
    const HistoryStats stats = m_history.stats(m_historyMetric);
    if (!stats.valid())
        return result;

    result.insert(QStringLiteral("min"), stats.min);
    result.insert(QStringLiteral("max"), stats.max);
    result.insert(QStringLiteral("avg"), stats.avg);
    result.insert(QStringLiteral("count"), stats.count);
    return result;
}

QString BatteryProvider::historyMetric() const
{
    return m_historyMetric;
}

void BatteryProvider::setHistoryMetric(const QString &metric)
{
    static const QStringList metrics = {QStringLiteral("voltage"),
                                        QStringLiteral("current"),
                                        QStringLiteral("power"),
                                        QStringLiteral("percentage")};
    if (!metrics.contains(metric) || metric == m_historyMetric)
        return;

    m_historyMetric = metric;
    emit historyMetricChanged();
    emit changed();
}

QStringList BatteryProvider::warnings() const
{
    return m_warnings;
}

QString BatteryProvider::sysfsStatus() const
{
    return m_sysfsStatus;
}

QString BatteryProvider::upowerStatus() const
{
    return m_upowerStatus;
}

qint64 BatteryProvider::lastUpdatedMs() const
{
    return m_lastUpdatedMs;
}

int BatteryProvider::pollInterval() const
{
    return m_pollIntervalMs;
}

void BatteryProvider::setPollInterval(int ms)
{
    const int clamped = qBound(MinPollIntervalMs, ms, MaxPollIntervalMs);
    if (clamped == m_pollIntervalMs)
        return;

    m_pollIntervalMs = clamped;
    const bool active = m_timer.isActive();
    m_timer.setInterval(m_pollIntervalMs);
    if (active)
        m_timer.start();
    emit pollIntervalChanged();
}

void BatteryProvider::refresh()
{
    m_snapshot = m_reader.read();
    rebuildSummary();
    m_lastUpdatedMs = QDateTime::currentMSecsSinceEpoch();
    m_history.add(m_lastUpdatedMs, m_summary, m_pollIntervalMs);
    emit changed();
    m_upower->refresh();
}

QString BatteryProvider::saveReport(const QString &path) const
{
    QString target = path;
    if (target.isEmpty()) {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (base.isEmpty())
            return {};
        target = base + QLatin1String("/batteryinspector.sprainbrains/")
                + QLatin1String(ReportFileName);
    }

    if (!QDir().mkpath(QFileInfo(target).absolutePath()))
        return {};

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};

    const QByteArray payload = reportJson(true).toUtf8();
    if (file.write(payload) != payload.size())
        return {};

    file.close();
    return target;
}

QString BatteryProvider::reportJson(bool pretty) const
{
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return QString::fromUtf8(ReportWriter::toJson(QLatin1String(AppVersion),
                                                 m_snapshot,
                                                 m_summary,
                                                 m_upowerData,
                                                 m_warnings,
                                                 timestamp,
                                                 pretty));
}

void BatteryProvider::start()
{
    refresh();
    m_timer.start();
}

void BatteryProvider::stop()
{
    m_timer.stop();
}

void BatteryProvider::onTick()
{
    refresh();
}

void BatteryProvider::onUpowerUpdated(const QVariantMap &device)
{
    m_upowerData = device;
    m_upowerStatus = QStringLiteral("ok");
    rebuildSummary();
    m_lastUpdatedMs = QDateTime::currentMSecsSinceEpoch();
    emit changed();
}

void BatteryProvider::onUpowerUnavailable(const QString &reason)
{
    m_upowerData.clear();
    m_upowerStatus = reason == QLatin1String("no-battery") ? QStringLiteral("no-battery")
                                                        : QStringLiteral("unavailable");
    rebuildSummary();
    emit changed();
}

void BatteryProvider::rebuildSummary()
{
    const NormalizedMetrics metrics = Metrics::normalize(m_snapshot, m_upowerData);
    m_summary = metrics.values;
    m_warnings = m_snapshot.warnings + metrics.warnings;
    m_sysfsStatus = !m_snapshot.sysfsAvailable
            ? QStringLiteral("unavailable")
            : (m_snapshot.primaryName.isEmpty() ? QStringLiteral("no-battery")
                                                : QStringLiteral("ok"));
}

QVariantList BatteryProvider::buildRawAttributes() const
{
    QVariantList rows;
    for (const PowerSupply &supply : m_snapshot.supplies) {
        for (auto it = supply.attributes.constBegin(); it != supply.attributes.constEnd(); ++it) {
            QVariantMap row;
            row.insert(QStringLiteral("supply"), supply.name);
            row.insert(QStringLiteral("name"), it.key());
            row.insert(QStringLiteral("value"), it.value());
            row.insert(QStringLiteral("source"),
                       supply.fromUevent.contains(it.key()) ? QStringLiteral("uevent")
                                                            : QStringLiteral("sysfs"));
            rows.append(row);
        }
    }

    for (auto it = m_upowerData.constBegin(); it != m_upowerData.constEnd(); ++it) {
        QVariantMap row;
        row.insert(QStringLiteral("supply"), QStringLiteral("upower"));
        row.insert(QStringLiteral("name"), it.key());
        row.insert(QStringLiteral("value"), it.value().toString());
        row.insert(QStringLiteral("source"), QStringLiteral("upower"));
        rows.append(row);
    }

    return rows;
}

QVariantList BatteryProvider::buildSupplies() const
{
    QVariantList rows;
    for (const PowerSupply &supply : m_snapshot.supplies) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), supply.name);
        row.insert(QStringLiteral("type"), supply.type);
        row.insert(QStringLiteral("online"),
                   supply.attributes.value(QStringLiteral("online")) == QLatin1String("1"));
        row.insert(QStringLiteral("present"),
                   supply.attributes.value(QStringLiteral("present")) == QLatin1String("1"));
        rows.append(row);
    }
    return rows;
}
