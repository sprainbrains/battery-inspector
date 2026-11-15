#ifndef METRICS_H
#define METRICS_H

#include "sysfsreader.h"

#include <QStringList>
#include <QVariantMap>

struct NormalizedMetrics
{
    QVariantMap values; // normalized summary keys, absent when unavailable
    QStringList warnings;
};

namespace Metrics {
NormalizedMetrics normalize(const SupplySnapshot &snapshot, const QVariantMap &upower = {});
}

#endif // METRICS_H
