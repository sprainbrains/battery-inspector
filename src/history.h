#ifndef HISTORY_H
#define HISTORY_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct HistorySample
{
    qint64 timestampMs = 0;
    bool gapBefore = false;
    QVariantMap values; // metric -> double
};

struct HistoryStats
{
    double min = 0;
    double max = 0;
    double avg = 0;
    int count = 0;
    bool valid() const { return count > 0; }
};

class HistoryBuffer
{
public:
    explicit HistoryBuffer(qint64 windowMs = 5 * 60 * 1000, int maxSamples = 300);

    void add(qint64 timestampMs, const QVariantMap &values, qint64 expectedIntervalMs);
    void clear();
    QVector<HistorySample> samples() const;
    QVariantList toVariantList() const; // {t, gap, percentage, voltage, current, power, temperature}
    HistoryStats stats(const QString &metric) const;

private:
    qint64 m_windowMs;
    int m_maxSamples;
    QVector<HistorySample> m_samples;
};

#endif // HISTORY_H
