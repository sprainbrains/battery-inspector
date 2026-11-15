#include "history.h"

#include <QtTest>

class TestHistory : public QObject
{
    Q_OBJECT

private slots:
    void evictsByTimeWindow();
    void capsAtMaxSamples();
    void marksGapAfterLongPause();
    void keepsHistoryWhenIntervalChanges();
    void computesStatsForMetric();
    void emptyStatsInvalid();
    void exportsGraphRows();
    void clearRemovesSamples();
};

void TestHistory::evictsByTimeWindow()
{
    HistoryBuffer history;
    history.add(0, {{"percentage", 50.0}}, 60000);
    history.add(60000, {{"percentage", 49.0}}, 60000);
    history.add(240000, {{"percentage", 48.0}}, 60000);
    history.add(301000, {{"percentage", 47.0}}, 60000);

    const auto samples = history.samples();
    QCOMPARE(samples.size(), 3);
    QCOMPARE(samples.at(0).timestampMs, qint64(60000));
    QCOMPARE(samples.at(1).timestampMs, qint64(240000));
    QCOMPARE(samples.at(2).timestampMs, qint64(301000));
}

void TestHistory::capsAtMaxSamples()
{
    HistoryBuffer history(1000000, 3);
    for (int i = 0; i < 5; ++i)
        history.add(i * 2000, {{"percentage", double(i)}}, 2000);

    const auto samples = history.samples();
    QCOMPARE(samples.size(), 3);
    QCOMPARE(samples.at(0).timestampMs, qint64(4000));
    QCOMPARE(samples.at(2).timestampMs, qint64(8000));
}

void TestHistory::marksGapAfterLongPause()
{
    HistoryBuffer history;
    history.add(0, {{"percentage", 50.0}}, 2000);
    history.add(10000, {{"percentage", 49.0}}, 2000);

    const auto samples = history.samples();
    QCOMPARE(samples.size(), 2);
    QCOMPARE(samples.at(0).gapBefore, false);
    QCOMPARE(samples.at(1).gapBefore, true);
}

void TestHistory::keepsHistoryWhenIntervalChanges()
{
    HistoryBuffer history;
    history.add(0, {{"percentage", 50.0}}, 2000);
    history.add(2000, {{"percentage", 49.0}}, 2000);
    history.add(4000, {{"percentage", 48.0}}, 2000);
    history.add(6000, {{"percentage", 47.0}}, 5000);

    const auto samples = history.samples();
    QCOMPARE(samples.size(), 4);
    QCOMPARE(samples.at(3).timestampMs, qint64(6000));
    QCOMPARE(samples.at(3).gapBefore, false);
}

void TestHistory::computesStatsForMetric()
{
    HistoryBuffer history;
    history.add(0, {{"voltage", 3.9}}, 1000);
    history.add(1000, {{"voltage", 4.0}}, 1000);
    history.add(2000, {{"voltage", 4.1}}, 1000);
    history.add(3000, {{"percentage", 42.0}}, 1000); // no voltage

    const HistoryStats voltage = history.stats("voltage");
    QVERIFY(voltage.valid());
    QCOMPARE(voltage.count, 3);
    QCOMPARE(voltage.min, 3.9);
    QCOMPARE(voltage.max, 4.1);
    QCOMPARE(voltage.avg, 4.0);
}

void TestHistory::emptyStatsInvalid()
{
    HistoryBuffer history;
    const HistoryStats stats = history.stats("voltage");
    QCOMPARE(stats.valid(), false);
    QCOMPARE(stats.count, 0);
}

void TestHistory::exportsGraphRows()
{
    HistoryBuffer history;
    history.add(1000, {{"percentage", 78.0}, {"voltage", 3.92}, {"current", -0.612},
                       {"power", -2.4}, {"temperature", 31.8},
                       {"statusKey", "discharging"}, {"cycleCount", 214}}, 5000);
    history.add(2000, {{"percentage", 77.0}}, 5000);

    const QVariantList rows = history.toVariantList();
    QCOMPARE(rows.size(), 2);

    const QVariantMap full = rows.at(0).toMap();
    QCOMPARE(full.size(), 7);
    QCOMPARE(full.value("t").toLongLong(), qint64(1000));
    QCOMPARE(full.value("gap").toBool(), false);
    QCOMPARE(full.value("percentage").toDouble(), 78.0);
    QCOMPARE(full.value("voltage").toDouble(), 3.92);
    QCOMPARE(full.value("current").toDouble(), -0.612);
    QCOMPARE(full.value("power").toDouble(), -2.4);
    QCOMPARE(full.value("temperature").toDouble(), 31.8);
    QVERIFY(!full.contains("statusKey"));
    QVERIFY(!full.contains("cycleCount"));

    const QVariantMap sparse = rows.at(1).toMap();
    QCOMPARE(sparse.size(), 3); // t, gap, percentage
    QCOMPARE(sparse.value("percentage").toDouble(), 77.0);
    QVERIFY(!sparse.contains("voltage"));
}

void TestHistory::clearRemovesSamples()
{
    HistoryBuffer history;
    history.add(0, {{"percentage", 50.0}}, 5000);
    history.clear();

    QVERIFY(history.samples().isEmpty());
    QVERIFY(history.toVariantList().isEmpty());
    QVERIFY(!history.stats("percentage").valid());
}

QTEST_GUILESS_MAIN(TestHistory)

#include "tst_history.moc"
