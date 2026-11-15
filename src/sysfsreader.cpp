#include "sysfsreader.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtGlobal>

#include <algorithm>

namespace {

constexpr qint64 MaxAttributeBytes = 4096;

QString readAttribute(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        return {};
    }

    const QByteArray data = file.read(MaxAttributeBytes);
    if (data.contains('\0')) {
        if (ok)
            *ok = false;
        return {};
    }

    if (ok)
        *ok = true;
    return QString::fromUtf8(data).trimmed();
}

} // namespace

SysfsReader::SysfsReader(const QString &root)
    : m_root(root.isEmpty() ? defaultRoot() : root)
{
}

SupplySnapshot SysfsReader::read() const
{
    SupplySnapshot snapshot;
    snapshot.sysfsRoot = m_root;

    if (!QFileInfo(m_root).isDir()) {
        snapshot.warnings << QStringLiteral("sysfs-unavailable");
        return snapshot;
    }
    snapshot.sysfsAvailable = true;

    const QDir rootDir(m_root);
    const QFileInfoList entries =
            rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QDir supplyDir(entry.absoluteFilePath());
        PowerSupply supply;
        supply.name = entry.fileName();

        bool anyRead = false;
        QString ueventText;
        const QFileInfoList files =
                supplyDir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo &fileInfo : files) {
            bool ok = false;
            const QString value = readAttribute(fileInfo.absoluteFilePath(), &ok);
            if (!ok)
                continue;

            anyRead = true;
            if (fileInfo.fileName() == QLatin1String("uevent")) {
                ueventText = value;
                continue;
            }
            supply.attributes.insert(fileInfo.fileName(), value);
        }
        supply.readable = anyRead;

        const QMap<QString, QString> ueventValues = parseUevent(ueventText);
        for (auto it = ueventValues.constBegin(); it != ueventValues.constEnd(); ++it) {
            if (supply.attributes.contains(it.key()))
                continue;
            supply.attributes.insert(it.key(), it.value());
            supply.fromUevent.insert(it.key());
        }

        supply.type = supply.attributes.value(QStringLiteral("type"));
        snapshot.supplies.append(supply);
    }

    for (const PowerSupply &supply : qAsConst(snapshot.supplies)) {
        if (snapshot.primaryName.isEmpty() && supply.type == QLatin1String("Battery"))
            snapshot.primaryName = supply.name;
        if (snapshot.bmsName.isEmpty()
            && (supply.type == QLatin1String("BMS")
                || supply.name == QLatin1String("bms"))) {
            snapshot.bmsName = supply.name;
        }
    }

    const QString primaryName = snapshot.primaryName;
    const QString bmsName = snapshot.bmsName;
    std::stable_sort(snapshot.supplies.begin(), snapshot.supplies.end(),
                     [&primaryName, &bmsName](const PowerSupply &left,
                                              const PowerSupply &right) {
        const auto rank = [&primaryName, &bmsName](const PowerSupply &supply) {
            if (!primaryName.isEmpty() && supply.name == primaryName)
                return 0;
            if (!bmsName.isEmpty() && supply.name == bmsName)
                return 1;
            return 2;
        };
        return rank(left) < rank(right);
    });

    if (snapshot.primaryName.isEmpty())
        snapshot.warnings << QStringLiteral("no-battery");

    return snapshot;
}

QString SysfsReader::root() const
{
    return m_root;
}

QString SysfsReader::defaultRoot()
{
    return qEnvironmentVariable("BATTERY_SYSFS_ROOT",
                                QStringLiteral("/sys/class/power_supply"));
}

QMap<QString, QString> SysfsReader::parseUevent(const QString &text)
{
    QMap<QString, QString> values;
    static const QString prefix = QStringLiteral("POWER_SUPPLY_");

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        const int separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0)
            continue;

        QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        if (key.startsWith(prefix))
            key = key.mid(prefix.size());
        key = key.toLower();
        if (key.isEmpty())
            continue;

        values.insert(key, value);
    }

    return values;
}
