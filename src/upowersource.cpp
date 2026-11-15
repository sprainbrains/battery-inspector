#include "upowersource.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDebug>
#include <QHash>
#include <QPointer>

#include <thread>

namespace {

constexpr int CallTimeoutMs = 1000;

const char *const UPowerService = "org.freedesktop.UPower";
const char *const UPowerPath = "/org/freedesktop/UPower";
const char *const UPowerInterface = "org.freedesktop.UPower";
const char *const PropertiesInterface = "org.freedesktop.DBus.Properties";
const char *const DeviceInterface = "org.freedesktop.UPower.Device";

QString withoutSeparators(const QString &key)
{
    QString result;
    result.reserve(key.size());
    for (const QChar character : key) {
        if (character == QLatin1Char('-') || character == QLatin1Char('_'))
            continue;
        result.append(character.toLower());
    }
    return result;
}

QString camelCaseKey(const QString &key)
{
    QString result;
    result.reserve(key.size());
    bool upperNext = false;
    for (const QChar character : key) {
        if (character == QLatin1Char('-') || character == QLatin1Char('_')) {
            upperNext = !result.isEmpty();
            continue;
        }
        if (result.isEmpty())
            result.append(character.toLower());
        else if (upperNext)
            result.append(character.toUpper());
        else
            result.append(character);
        upperNext = false;
    }
    return result;
}

// Returns the canonical name for a known property, empty for unknown ones.
QString knownPropertyName(const QString &normalizedKey)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("percentage"), QStringLiteral("percentage")},
        {QStringLiteral("state"), QStringLiteral("state")},
        {QStringLiteral("capacity"), QStringLiteral("capacity")},
        {QStringLiteral("chargecycles"), QStringLiteral("chargeCycles")},
        {QStringLiteral("voltage"), QStringLiteral("voltage")},
        {QStringLiteral("energy"), QStringLiteral("energy")},
        {QStringLiteral("energyfull"), QStringLiteral("energyFull")},
        {QStringLiteral("energyfulldesign"), QStringLiteral("energyFullDesign")},
        {QStringLiteral("energyrate"), QStringLiteral("energyRate")},
        {QStringLiteral("temperature"), QStringLiteral("temperature")},
        {QStringLiteral("timetoempty"), QStringLiteral("timeToEmpty")},
        {QStringLiteral("timetofull"), QStringLiteral("timeToFull")},
        {QStringLiteral("ispresent"), QStringLiteral("isPresent")},
        {QStringLiteral("technology"), QStringLiteral("technology")},
        {QStringLiteral("warninglevel"), QStringLiteral("warningLevel")},
        {QStringLiteral("batterylevel"), QStringLiteral("batteryLevel")},
        {QStringLiteral("model"), QStringLiteral("model")},
        {QStringLiteral("vendor"), QStringLiteral("vendor")},
        {QStringLiteral("serial"), QStringLiteral("serial")},
        {QStringLiteral("nativepath"), QStringLiteral("nativePath")},
        {QStringLiteral("chargestartthreshold"), QStringLiteral("chargeStartThreshold")},
        {QStringLiteral("chargeendthreshold"), QStringLiteral("chargeEndThreshold")},
        {QStringLiteral("chargethresholdsupported"),
         QStringLiteral("chargeThresholdSupported")},
        {QStringLiteral("voltagemindesign"), QStringLiteral("voltageMinDesign")},
        {QStringLiteral("voltagemaxdesign"), QStringLiteral("voltageMaxDesign")},
    };
    return names.value(normalizedKey);
}

QVariant unwrapValue(const QVariant &value)
{
    if (value.userType() == qMetaTypeId<QDBusVariant>())
        return value.value<QDBusVariant>().variant();
    return value;
}

QVariant technologyValue(const QVariant &value)
{
    bool ok = false;
    const int enumValue = value.toInt(&ok);
    if (!ok)
        return value; // already a human readable string

    switch (enumValue) {
    case 1:
        return QStringLiteral("Li-ion");
    case 2:
        return QStringLiteral("Li-poly");
    case 3:
        return QStringLiteral("LiFePO4");
    case 4:
        return QStringLiteral("Lead acid");
    case 5:
        return QStringLiteral("NiCd");
    case 6:
        return QStringLiteral("NiMH");
    default:
        return QStringLiteral("Unknown");
    }
}

} // namespace

UpowerSource::UpowerSource(QObject *parent)
    : QObject(parent)
{
}

QDBusMessage UpowerSource::enumerateDevicesRequest()
{
    return QDBusMessage::createMethodCall(QLatin1String(UPowerService),
                                          QLatin1String(UPowerPath),
                                          QLatin1String(UPowerInterface),
                                          QStringLiteral("EnumerateDevices"));
}

QDBusMessage UpowerSource::devicePropertiesRequest(const QString &path)
{
    QDBusMessage request = QDBusMessage::createMethodCall(QLatin1String(UPowerService), path,
                                                          QLatin1String(PropertiesInterface),
                                                          QStringLiteral("GetAll"));
    request << QLatin1String(DeviceInterface);
    return request;
}

QVariantMap UpowerSource::mapProperties(const QVariantMap &raw)
{
    QVariantMap mapped;
    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        QVariant value = unwrapValue(it.value());
        if (!value.isValid())
            continue;

        const QString normalizedKey = withoutSeparators(it.key());
        if (normalizedKey.isEmpty())
            continue;

        const QString knownKey = knownPropertyName(normalizedKey);
        const QString targetKey = knownKey.isEmpty() ? camelCaseKey(it.key()) : knownKey;

        if (targetKey == QLatin1String("chargeCycles")) {
            bool ok = false;
            const int cycles = value.toInt(&ok);
            if (ok && cycles < 0)
                continue; // UPower uses a negative value for "unknown"
        }
        if (targetKey == QLatin1String("technology"))
            value = technologyValue(value);

        mapped.insert(targetKey, value);
    }
    return mapped;
}

bool UpowerSource::isBatteryDevice(const QVariantMap &props)
{
    QVariant type = props.value(QStringLiteral("type"));
    if (!type.isValid())
        type = props.value(QStringLiteral("Type"));
    type = unwrapValue(type);
    if (!type.isValid())
        return false;

    bool ok = false;
    const int value = type.toInt(&ok);
    return ok && value == 2;
}

RealUpowerSource::RealUpowerSource(QObject *parent)
    : UpowerSource(parent)
{
}

void RealUpowerSource::refresh()
{
    if (m_busy)
        return;

    if (!QDBusConnection::systemBus().isConnected()) {
        emit unavailable(QStringLiteral("bus-unavailable"));
        return;
    }

    // The synchronous GetAll path is the one exercised by --dump and by the
    // confined AppArmor profile. Run it on a worker so the GUI never blocks.
    m_busy = true;
    QPointer<RealUpowerSource> self(this);
    std::thread([self]() {
        if (!self)
            return;

        const QVariantMap data = self->blockingRead(1000);
        if (!self)
            return;

        QMetaObject::invokeMethod(self.data(), [self, data]() {
            if (!self)
                return;
            self->m_busy = false;
            if (data.isEmpty())
                emit self->unavailable(QStringLiteral("unavailable"));
            else
                emit self->updated(data);
        }, Qt::QueuedConnection);
    }).detach();
}

QVariantMap RealUpowerSource::blockingRead(int timeoutMs) const
{
    const QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected())
        return {};

    const QDBusMessage reply
            = bus.call(enumerateDevicesRequest(), QDBus::Block, timeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return {};

    const QList<QDBusObjectPath> paths
            = qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().constFirst());
    for (const QDBusObjectPath &path : paths) {
        const QDBusMessage deviceReply
                = bus.call(devicePropertiesRequest(path.path()), QDBus::Block, timeoutMs);
        if (deviceReply.type() != QDBusMessage::ReplyMessage
            || deviceReply.arguments().isEmpty()) {
            continue;
        }

        const QVariantMap raw = qdbus_cast<QVariantMap>(deviceReply.arguments().constFirst());
        if (!isBatteryDevice(raw))
            continue;

        return mapProperties(raw);
    }
    return {};
}
