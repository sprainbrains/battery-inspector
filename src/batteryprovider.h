#ifndef BATTERYPROVIDER_H
#define BATTERYPROVIDER_H

#include "history.h"
#include "sysfsreader.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class UpowerSource;

class BatteryProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
    Q_PROPERTY(QVariantMap upower READ upower NOTIFY changed)
    Q_PROPERTY(QVariantList rawAttributes READ rawAttributes NOTIFY changed)
    Q_PROPERTY(QVariantList supplies READ supplies NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QVariantMap historyStats READ historyStats NOTIFY changed)
    Q_PROPERTY(QString historyMetric READ historyMetric WRITE setHistoryMetric NOTIFY historyMetricChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY changed)
    Q_PROPERTY(QString sysfsStatus READ sysfsStatus NOTIFY changed)
    Q_PROPERTY(QString upowerStatus READ upowerStatus NOTIFY changed)
    Q_PROPERTY(qint64 lastUpdatedMs READ lastUpdatedMs NOTIFY changed)
    Q_PROPERTY(int pollInterval READ pollInterval WRITE setPollInterval NOTIFY pollIntervalChanged)

public:
    explicit BatteryProvider(UpowerSource *upower = nullptr, QObject *parent = nullptr);
    ~BatteryProvider() override;

    QVariantMap summary() const;
    QVariantMap upower() const;
    QVariantList rawAttributes() const;
    QVariantList supplies() const;
    QVariantList history() const;
    QVariantMap historyStats() const;
    QString historyMetric() const;
    void setHistoryMetric(const QString &metric);
    QStringList warnings() const;
    QString sysfsStatus() const;
    QString upowerStatus() const;
    qint64 lastUpdatedMs() const;
    int pollInterval() const;
    void setPollInterval(int ms);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString saveReport(const QString &path = QString()) const;
    Q_INVOKABLE QString reportJson(bool pretty = true) const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void changed();
    void pollIntervalChanged();
    void historyMetricChanged();

private slots:
    void onTick();
    void onUpowerUpdated(const QVariantMap &device);
    void onUpowerUnavailable(const QString &reason);

private:
    void rebuildSummary();
    QVariantList buildRawAttributes() const;
    QVariantList buildSupplies() const;

    SysfsReader m_reader;
    UpowerSource *m_upower;
    QTimer m_timer;
    HistoryBuffer m_history;
    SupplySnapshot m_snapshot;
    QVariantMap m_summary;
    QVariantMap m_upowerData;
    QStringList m_warnings;
    QString m_sysfsStatus = QStringLiteral("unavailable");
    QString m_upowerStatus = QStringLiteral("unavailable");
    QString m_historyMetric = QStringLiteral("voltage");
    qint64 m_lastUpdatedMs = 0;
    int m_pollIntervalMs = 2000;
};

#endif // BATTERYPROVIDER_H
