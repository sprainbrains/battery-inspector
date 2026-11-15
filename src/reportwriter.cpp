#include "reportwriter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>
#include <QtGlobal>

namespace {

QVariantMap sanitizeMap(const QVariantMap &map);
bool sanitizeVariant(QVariant *value);

QVariantList sanitizeList(const QVariantList &list)
{
    QVariantList result;
    for (const QVariant &value : list) {
        QVariant copy = value;
        if (sanitizeVariant(&copy))
            result.append(copy);
    }
    return result;
}

QVariantMap sanitizeMap(const QVariantMap &map)
{
    QVariantMap result;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        QVariant value = it.value();
        if (sanitizeVariant(&value))
            result.insert(it.key(), value);
    }
    return result;
}

// JSON cannot represent NaN or infinities, so such values are omitted entirely.
bool sanitizeVariant(QVariant *value)
{
    switch (value->type()) {
    case QVariant::Double:
        return qIsFinite(value->toDouble());
    case QVariant::Map:
        *value = sanitizeMap(value->toMap());
        return true;
    case QVariant::List:
        *value = sanitizeList(value->toList());
        return true;
    default:
        return true;
    }
}

QVariantMap supplyToVariantMap(const PowerSupply &supply)
{
    QVariantMap attributes;
    for (auto it = supply.attributes.constBegin(); it != supply.attributes.constEnd(); ++it) {
        QVariantMap attribute;
        attribute.insert(QStringLiteral("value"), it.value());
        attribute.insert(QStringLiteral("source"),
                         supply.fromUevent.contains(it.key()) ? QStringLiteral("uevent")
                                                              : QStringLiteral("sysfs"));
        attributes.insert(it.key(), attribute);
    }

    QVariantMap result;
    result.insert(QStringLiteral("name"), supply.name);
    result.insert(QStringLiteral("type"), supply.type);
    result.insert(QStringLiteral("attributes"), attributes);
    return result;
}

} // namespace

namespace ReportWriter {

QVariantMap toVariantMap(const QString &appVersion,
                         const SupplySnapshot &snapshot,
                         const QVariantMap &summary,
                         const QVariantMap &upower,
                         const QStringList &warnings,
                         const QString &timestampIso)
{
    QVariantList supplies;
    supplies.reserve(snapshot.supplies.size());
    for (const PowerSupply &supply : snapshot.supplies)
        supplies.append(supplyToVariantMap(supply));

    QVariantMap report;
    report.insert(QStringLiteral("schemaVersion"), 1);
    report.insert(QStringLiteral("app"), QStringLiteral("Battery Inspector"));
    report.insert(QStringLiteral("appVersion"), appVersion);
    report.insert(QStringLiteral("timestamp"), timestampIso);
    report.insert(QStringLiteral("sysfsRoot"), snapshot.sysfsRoot);
    report.insert(QStringLiteral("sysfsAvailable"), snapshot.sysfsAvailable);
    report.insert(QStringLiteral("supplies"), supplies);
    report.insert(QStringLiteral("summary"), sanitizeMap(summary));
    report.insert(QStringLiteral("upower"), sanitizeMap(upower));
    report.insert(QStringLiteral("warnings"), warnings);
    return report;
}

QByteArray toJson(const QString &appVersion,
                  const SupplySnapshot &snapshot,
                  const QVariantMap &summary,
                  const QVariantMap &upower,
                  const QStringList &warnings,
                  const QString &timestampIso,
                  bool pretty)
{
    const QJsonObject report = QJsonObject::fromVariantMap(
            toVariantMap(appVersion, snapshot, summary, upower, warnings, timestampIso));
    return QJsonDocument(report).toJson(pretty ? QJsonDocument::Indented
                                               : QJsonDocument::Compact);
}

} // namespace ReportWriter
