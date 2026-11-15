#ifndef SYSFSREADER_H
#define SYSFSREADER_H

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

struct PowerSupply
{
    QString name;
    QString type;
    QMap<QString, QString> attributes; // raw key -> raw value
    QSet<QString> fromUevent;          // keys whose value came from uevent
    bool readable = false;
};

struct SupplySnapshot
{
    QVector<PowerSupply> supplies;
    QString primaryName;
    QString bmsName;
    QString sysfsRoot;
    bool sysfsAvailable = false;
    QStringList warnings; // "sysfs-unavailable", "no-battery"
};

class SysfsReader
{
public:
    explicit SysfsReader(const QString &root = QString());

    SupplySnapshot read() const;
    QString root() const;

    static QString defaultRoot();
    static QMap<QString, QString> parseUevent(const QString &text);

private:
    QString m_root;
};

#endif // SYSFSREADER_H
