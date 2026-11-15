#ifndef UPOWERSOURCE_H
#define UPOWERSOURCE_H

#include <QDBusMessage>
#include <QObject>
#include <QVariantMap>

// Reads the UPower daemon over D-Bus. UPower is optional: every failure is
// reported through unavailable() instead of throwing or blocking the caller.
class UpowerSource : public QObject
{
    Q_OBJECT

public:
    explicit UpowerSource(QObject *parent = nullptr);
    virtual ~UpowerSource() = default;

    // Asynchronous; emits updated() or unavailable() later.
    virtual void refresh() = 0;
    // Synchronous read for --dump. Returns an empty map on any failure.
    virtual QVariantMap blockingRead(int timeoutMs = 1000) const = 0;

    // Normalizes raw UPower Device properties to camelCase keys.
    static QVariantMap mapProperties(const QVariantMap &raw);
    // True when the device reports Type == 2 (battery).
    static bool isBatteryDevice(const QVariantMap &props);

    // Request builders exposed for tests. GetAll must be sent to the standard
    // org.freedesktop.DBus.Properties interface, not to the device interface.
    static QDBusMessage enumerateDevicesRequest();
    static QDBusMessage devicePropertiesRequest(const QString &path);

signals:
    void updated(const QVariantMap &device);
    void unavailable(const QString &reason); // "bus-unavailable", "service-unavailable", "no-battery"
};

class RealUpowerSource : public UpowerSource
{
    Q_OBJECT

public:
    explicit RealUpowerSource(QObject *parent = nullptr);

    void refresh() override;
    QVariantMap blockingRead(int timeoutMs = 1000) const override;

private:
    bool m_busy = false;
};

#endif // UPOWERSOURCE_H
