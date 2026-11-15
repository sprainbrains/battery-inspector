#include "history.h"

#include <QStringList>
#include <QtGlobal>

namespace {

const QStringList &graphMetrics()
{
    static const QStringList metrics = {
        QStringLiteral("percentage"),
        QStringLiteral("voltage"),
        QStringLiteral("current"),
        QStringLiteral("power"),
        QStringLiteral("temperature"),
    };
    return metrics;
}

} // namespace

HistoryBuffer::HistoryBuffer(qint64 windowMs, int maxSamples)
    : m_windowMs(windowMs)
    , m_maxSamples(maxSamples)
{
}

void HistoryBuffer::add(qint64 timestampMs, const QVariantMap &values, qint64 expectedIntervalMs)
{
    const qint64 oldest = timestampMs - m_windowMs;
    while (!m_samples.isEmpty() && m_samples.first().timestampMs < oldest)
        m_samples.removeFirst();

    HistorySample sample;
    sample.timestampMs = timestampMs;
    sample.values = values;
    if (!m_samples.isEmpty())
        sample.gapBefore = timestampMs - m_samples.last().timestampMs > 2 * expectedIntervalMs;

    m_samples.append(sample);

    while (m_samples.size() > m_maxSamples)
        m_samples.removeFirst();
}

void HistoryBuffer::clear()
{
    m_samples.clear();
}

QVector<HistorySample> HistoryBuffer::samples() const
{
    return m_samples;
}

QVariantList HistoryBuffer::toVariantList() const
{
    QVariantList rows;
    rows.reserve(m_samples.size());
    for (const HistorySample &sample : m_samples) {
        QVariantMap row;
        row.insert(QStringLiteral("t"), sample.timestampMs);
        row.insert(QStringLiteral("gap"), sample.gapBefore);
        for (const QString &metric : graphMetrics()) {
            const auto it = sample.values.constFind(metric);
            if (it != sample.values.constEnd())
                row.insert(metric, it.value());
        }
        rows.append(row);
    }
    return rows;
}

HistoryStats HistoryBuffer::stats(const QString &metric) const
{
    HistoryStats stats;
    double sum = 0;
    for (const HistorySample &sample : m_samples) {
        const QVariant value = sample.values.value(metric);
        if (value.userType() != QMetaType::Double)
            continue;
        const double v = value.toDouble();
        if (stats.count == 0) {
            stats.min = v;
            stats.max = v;
        } else {
            stats.min = qMin(stats.min, v);
            stats.max = qMax(stats.max, v);
        }
        sum += v;
        ++stats.count;
    }
    if (stats.valid())
        stats.avg = sum / stats.count;
    return stats;
}
