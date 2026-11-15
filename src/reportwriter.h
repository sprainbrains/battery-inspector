#ifndef REPORTWRITER_H
#define REPORTWRITER_H

#include "sysfsreader.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace ReportWriter {

QVariantMap toVariantMap(const QString &appVersion,
                         const SupplySnapshot &snapshot,
                         const QVariantMap &summary,
                         const QVariantMap &upower,
                         const QStringList &warnings,
                         const QString &timestampIso);

QByteArray toJson(const QString &appVersion,
                  const SupplySnapshot &snapshot,
                  const QVariantMap &summary,
                  const QVariantMap &upower,
                  const QStringList &warnings,
                  const QString &timestampIso,
                  bool pretty);

} // namespace ReportWriter

#endif // REPORTWRITER_H
