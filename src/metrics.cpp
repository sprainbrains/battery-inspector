#include "metrics.h"

#include <QtGlobal>

#include <limits>

namespace {

constexpr double Micro = 1e6;   // µV, µA, µAh, µWh, µΩ → SI
constexpr double DeciDegree = 10.0; // 0.1 °C → °C

const PowerSupply *supplyByName(const SupplySnapshot &snapshot, const QString &name)
{
    if (name.isEmpty())
        return nullptr;
    for (const PowerSupply &supply : snapshot.supplies) {
        if (supply.name == name)
            return &supply;
    }
    return nullptr;
}

struct Sources
{
    QVector<const PowerSupply *> battery; // primary, then BMS
    QVector<const PowerSupply *> all;     // battery, then chargers/others
};

Sources collectSources(const SupplySnapshot &snapshot)
{
    Sources sources;

    const PowerSupply *primary = supplyByName(snapshot, snapshot.primaryName);
    if (!primary) {
        for (const PowerSupply &supply : snapshot.supplies) {
            if (supply.type == QLatin1String("Battery")) {
                primary = &supply;
                break;
            }
        }
    }

    const PowerSupply *bms = supplyByName(snapshot, snapshot.bmsName);
    if (!bms) {
        for (const PowerSupply &supply : snapshot.supplies) {
            if (&supply == primary)
                continue;
            if (supply.type == QLatin1String("BMS")
                || supply.name == QLatin1String("bms")) {
                bms = &supply;
                break;
            }
        }
    }

    if (primary)
        sources.battery << primary;
    if (bms && bms != primary)
        sources.battery << bms;
    sources.all = sources.battery;
    for (const PowerSupply &supply : snapshot.supplies) {
        if (!sources.all.contains(&supply))
            sources.all << &supply;
    }
    return sources;
}

bool numberFromAttributes(const QVector<const PowerSupply *> &supplies,
                          const QString &key, double *out)
{
    for (const PowerSupply *supply : supplies) {
        const auto it = supply->attributes.constFind(key);
        if (it == supply->attributes.constEnd())
            continue;
        bool ok = false;
        const double value = it.value().toDouble(&ok);
        if (!ok || !qIsFinite(value))
            continue; // empty or junk: keep looking in lower-priority supplies
        *out = value;
        return true;
    }
    return false;
}

bool stringFromAttributes(const QVector<const PowerSupply *> &supplies,
                          const QString &key, QString *out)
{
    for (const PowerSupply *supply : supplies) {
        const auto it = supply->attributes.constFind(key);
        if (it == supply->attributes.constEnd())
            continue;
        const QString value = it.value().trimmed();
        if (value.isEmpty())
            continue;
        *out = value;
        return true;
    }
    return false;
}

bool numberFromUpower(const QVariantMap &upower, const QString &key, double *out)
{
    const auto it = upower.constFind(key);
    if (it == upower.constEnd())
        return false;
    bool ok = false;
    const double value = it.value().toDouble(&ok);
    if (!ok || !qIsFinite(value))
        return false;
    *out = value;
    return true;
}

double clampPercent(double value)
{
    return qBound(0.0, value, 100.0);
}

QString statusKeyFor(const QString &status)
{
    const QString normalized = status.trimmed().toLower();
    if (normalized == QLatin1String("charging"))
        return QStringLiteral("charging");
    if (normalized == QLatin1String("discharging"))
        return QStringLiteral("discharging");
    if (normalized == QLatin1String("full"))
        return QStringLiteral("full");
    if (normalized == QLatin1String("not charging"))
        return QStringLiteral("notCharging");
    return QStringLiteral("unknown");
}

QString statusKeyFromUpowerState(int state)
{
    switch (state) {
    case 1: // charging
    case 5: // pending charge
        return QStringLiteral("charging");
    case 2: // discharging
    case 3: // empty
    case 6: // pending discharge
        return QStringLiteral("discharging");
    case 4: // full
        return QStringLiteral("full");
    default:
        return {};
    }
}

QString technologyName(int value)
{
    switch (value) {
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
        return {};
    }
}

bool technologyFromUpower(const QVariantMap &upower, QString *out)
{
    const auto it = upower.constFind(QStringLiteral("technology"));
    if (it == upower.constEnd())
        return false;

    bool ok = false;
    const int enumValue = it.value().toInt(&ok);
    if (ok) {
        const QString name = technologyName(enumValue);
        if (name.isEmpty())
            return false;
        *out = name;
        return true;
    }

    const QString text = it.value().toString().trimmed();
    if (text.isEmpty())
        return false;
    *out = text;
    return true;
}

bool secondsToInt(double seconds, int *out)
{
    if (!(seconds > 0.0) || seconds > double(std::numeric_limits<int>::max()))
        return false;
    *out = qRound(seconds);
    return true;
}

bool countToInt(double count, int *out)
{
    if (count < 0.0 || count > double(std::numeric_limits<int>::max()))
        return false;
    *out = qRound(count);
    return true;
}

} // namespace

namespace Metrics {

NormalizedMetrics normalize(const SupplySnapshot &snapshot, const QVariantMap &upower)
{
    NormalizedMetrics result;

    const Sources sources = collectSources(snapshot);
    QVariantMap &values = result.values;

    double value = 0.0;

    // Percentage: sysfs capacity → UPower percentage → charge_now / charge_full.
    if (numberFromAttributes(sources.battery, QStringLiteral("capacity"), &value)) {
        values.insert(QStringLiteral("percentage"), clampPercent(value));
    } else if (numberFromUpower(upower, QStringLiteral("percentage"), &value)) {
        values.insert(QStringLiteral("percentage"), clampPercent(value));
    } else {
        double now = 0.0;
        double full = 0.0;
        if (numberFromAttributes(sources.battery, QStringLiteral("charge_now"), &now)
            && numberFromAttributes(sources.battery, QStringLiteral("charge_full"), &full)
            && full > 0.0) {
            values.insert(QStringLiteral("percentage"), clampPercent(now * 100.0 / full));
        }
    }

    // Status: raw sysfs string plus a stable key for the UI.
    QString text;
    if (stringFromAttributes(sources.battery, QStringLiteral("status"), &text)) {
        values.insert(QStringLiteral("status"), text);
        values.insert(QStringLiteral("statusKey"), statusKeyFor(text));
    }
    if (values.value(QStringLiteral("statusKey")).toString().isEmpty()
        || values.value(QStringLiteral("statusKey")).toString()
                == QLatin1String("unknown")) {
        bool ok = false;
        const int upowerState = upower.value(QStringLiteral("state")).toInt(&ok);
        if (ok) {
            const QString mapped = statusKeyFromUpowerState(upowerState);
            if (!mapped.isEmpty())
                values.insert(QStringLiteral("statusKey"), mapped);
        }
    }

    // Voltage: voltage_now → voltage_avg → UPower voltage.
    bool hasVoltage = false;
    double voltage = 0.0;
    if (numberFromAttributes(sources.battery, QStringLiteral("voltage_now"), &value)) {
        voltage = value / Micro;
        hasVoltage = true;
    } else if (numberFromAttributes(sources.battery, QStringLiteral("voltage_avg"), &value)) {
        voltage = value / Micro;
        hasVoltage = true;
    } else if (numberFromUpower(upower, QStringLiteral("voltage"), &value)) {
        voltage = value;
        hasVoltage = true;
    }
    if (hasVoltage)
        values.insert(QStringLiteral("voltage"), voltage);

    // Current: current_now → current_avg; sign is preserved as read.
    bool hasCurrent = false;
    double current = 0.0;
    if (numberFromAttributes(sources.battery, QStringLiteral("current_now"), &value)) {
        current = value / Micro;
        hasCurrent = true;
    } else if (numberFromAttributes(sources.battery, QStringLiteral("current_avg"), &value)) {
        current = value / Micro;
        hasCurrent = true;
    }
    if (hasCurrent)
        values.insert(QStringLiteral("current"), current);

    // Power: power_now → voltage * current.
    bool hasPower = false;
    double power = 0.0;
    if (numberFromAttributes(sources.battery, QStringLiteral("power_now"), &value)) {
        power = value / Micro;
        hasPower = true;
    } else if (hasVoltage && hasCurrent) {
        power = voltage * current;
        hasPower = true;
    } else if (numberFromUpower(upower, QStringLiteral("energyRate"), &value)) {
        power = value;
        hasPower = true;
    }
    if (hasPower)
        values.insert(QStringLiteral("power"), power);

    // Temperature: temp / temp_now in 0.1 °C → UPower temperature.
    bool hasTemperature = false;
    double temperature = 0.0;
    if (numberFromAttributes(sources.battery, QStringLiteral("temp"), &value)) {
        temperature = value / DeciDegree;
        hasTemperature = true;
    } else if (numberFromAttributes(sources.battery, QStringLiteral("temp_now"), &value)) {
        temperature = value / DeciDegree;
        hasTemperature = true;
    } else if (numberFromUpower(upower, QStringLiteral("temperature"), &value)) {
        temperature = value;
        hasTemperature = true;
    }
    if (hasTemperature)
        values.insert(QStringLiteral("temperature"), temperature);

    // Health: charge_full / charge_full_design → energy equivalent → UPower capacity.
    bool hasHealth = false;
    double health = 0.0;
    double full = 0.0;
    double design = 0.0;
    if (numberFromAttributes(sources.battery, QStringLiteral("charge_full"), &full)
        && full > 0.0
        && numberFromAttributes(sources.battery, QStringLiteral("charge_full_design"), &design)
        && design > 0.0) {
        health = full * 100.0 / design;
        hasHealth = true;
    } else if (numberFromAttributes(sources.battery, QStringLiteral("energy_full"), &full)
               && full > 0.0
               && numberFromAttributes(sources.battery, QStringLiteral("energy_full_design"),
                                       &design)
               && design > 0.0) {
        health = full * 100.0 / design;
        hasHealth = true;
    } else if (numberFromUpower(upower, QStringLiteral("capacity"), &value) && value > 0.0) {
        health = value;
        hasHealth = true;
    }
    if (hasHealth) {
        values.insert(QStringLiteral("healthPercent"), health);
        values.insert(QStringLiteral("wearPercent"), qMax(0.0, 100.0 - health));
    }

    // Charge capacities in µAh → Ah.
    if (numberFromAttributes(sources.battery, QStringLiteral("charge_now"), &value))
        values.insert(QStringLiteral("chargeNowAh"), value / Micro);
    if (numberFromAttributes(sources.battery, QStringLiteral("charge_full"), &value))
        values.insert(QStringLiteral("chargeFullAh"), value / Micro);
    if (numberFromAttributes(sources.battery, QStringLiteral("charge_full_design"), &value))
        values.insert(QStringLiteral("chargeFullDesignAh"), value / Micro);
    if (numberFromAttributes(sources.battery, QStringLiteral("charge_counter"), &value))
        values.insert(QStringLiteral("chargeCounterAh"), value / Micro);

    // Energy capacities in µWh → Wh, with UPower fallbacks in target units.
    if (numberFromAttributes(sources.battery, QStringLiteral("energy_now"), &value)) {
        values.insert(QStringLiteral("energyNowWh"), value / Micro);
    } else if (numberFromUpower(upower, QStringLiteral("energy"), &value)) {
        values.insert(QStringLiteral("energyNowWh"), value);
    }
    if (numberFromAttributes(sources.battery, QStringLiteral("energy_full"), &value)) {
        values.insert(QStringLiteral("energyFullWh"), value / Micro);
    } else if (numberFromUpower(upower, QStringLiteral("energyFull"), &value)) {
        values.insert(QStringLiteral("energyFullWh"), value);
    }
    if (numberFromAttributes(sources.battery, QStringLiteral("energy_full_design"), &value)) {
        values.insert(QStringLiteral("energyFullDesignWh"), value / Micro);
    } else if (numberFromUpower(upower, QStringLiteral("energyFullDesign"), &value)) {
        values.insert(QStringLiteral("energyFullDesignWh"), value);
    }

    // Cycle count: sysfs → UPower, negative values mean unknown.
    int cycles = 0;
    if (numberFromAttributes(sources.battery, QStringLiteral("cycle_count"), &value)
        && countToInt(value, &cycles)) {
        values.insert(QStringLiteral("cycleCount"), cycles);
    } else if (numberFromUpower(upower, QStringLiteral("chargeCycles"), &value)
               && countToInt(value, &cycles)) {
        values.insert(QStringLiteral("cycleCount"), cycles);
    }

    // Time to empty: sysfs seconds → UPower seconds → charge/energy estimate.
    int seconds = 0;
    bool hasTimeToEmpty = false;
    if (numberFromAttributes(sources.battery, QStringLiteral("time_to_empty_avg"), &value)
        && secondsToInt(value, &seconds)) {
        values.insert(QStringLiteral("timeToEmptySec"), seconds);
        hasTimeToEmpty = true;
    } else if (numberFromUpower(upower, QStringLiteral("timeToEmpty"), &value)
               && secondsToInt(value, &seconds)) {
        values.insert(QStringLiteral("timeToEmptySec"), seconds);
        hasTimeToEmpty = true;
    }
    const QString state = values.value(QStringLiteral("statusKey")).toString();
    if (!hasTimeToEmpty && state == QLatin1String("discharging")) {
        double estimate = 0.0;
        bool hasEstimate = false;
        const double chargeNow = values.value(QStringLiteral("chargeNowAh")).toDouble();
        if (hasCurrent && current != 0.0 && chargeNow > 0.0) {
            estimate = chargeNow / qAbs(current) * 3600.0;
            hasEstimate = true;
        } else if (hasPower && power != 0.0
                   && values.contains(QStringLiteral("energyNowWh"))) {
            estimate = values.value(QStringLiteral("energyNowWh")).toDouble() / qAbs(power)
                    * 3600.0;
            hasEstimate = true;
        }
        if (hasEstimate && secondsToInt(estimate, &seconds))
            values.insert(QStringLiteral("timeToEmptySec"), seconds);
    }

    // Time to full: sysfs seconds → UPower seconds → remaining charge/energy estimate.
    int fullSeconds = 0;
    bool hasTimeToFull = false;
    if (numberFromAttributes(sources.battery, QStringLiteral("time_to_full_avg"), &value)
        && secondsToInt(value, &fullSeconds)) {
        values.insert(QStringLiteral("timeToFullSec"), fullSeconds);
        hasTimeToFull = true;
    } else if (numberFromUpower(upower, QStringLiteral("timeToFull"), &value)
               && secondsToInt(value, &fullSeconds)) {
        values.insert(QStringLiteral("timeToFullSec"), fullSeconds);
        hasTimeToFull = true;
    }
    if (!hasTimeToFull && state == QLatin1String("charging")) {
        double estimate = 0.0;
        bool hasEstimate = false;
        if (hasCurrent && current != 0.0
            && values.contains(QStringLiteral("chargeNowAh"))
            && values.contains(QStringLiteral("chargeFullAh"))) {
            const double remaining = values.value(QStringLiteral("chargeFullAh")).toDouble()
                    - values.value(QStringLiteral("chargeNowAh")).toDouble();
            if (remaining > 0.0) {
                estimate = remaining / qAbs(current) * 3600.0;
                hasEstimate = true;
            }
        } else if (hasPower && power != 0.0
                   && values.contains(QStringLiteral("energyNowWh"))
                   && values.contains(QStringLiteral("energyFullWh"))) {
            const double remaining = values.value(QStringLiteral("energyFullWh")).toDouble()
                    - values.value(QStringLiteral("energyNowWh")).toDouble();
            if (remaining > 0.0) {
                estimate = remaining / qAbs(power) * 3600.0;
                hasEstimate = true;
            }
        }
        if (hasEstimate && secondsToInt(estimate, &fullSeconds))
            values.insert(QStringLiteral("timeToFullSec"), fullSeconds);
    }

    // Technology: sysfs string → UPower enum/string.
    if (stringFromAttributes(sources.battery, QStringLiteral("technology"), &text)) {
        values.insert(QStringLiteral("technology"), text);
    } else if (technologyFromUpower(upower, &text)) {
        values.insert(QStringLiteral("technology"), text);
    }

    // Plain text fields read from the battery/BMS only, with UPower fallbacks.
    static const struct
    {
        const char *key;
        const char *attribute;
        const char *upowerKey; // nullptr when UPower has no matching property
    } textFields[] = {
        {"health", "health", nullptr},
        {"capacityLevel", "capacity_level", nullptr},
        {"chargeType", "charge_type", nullptr},
        {"modelName", "model_name", "model"},
        {"manufacturer", "manufacturer", "vendor"},
        {"serialNumber", "serial_number", "serial"},
        {"batteryType", "battery_type", nullptr},
    };
    for (const auto &field : textFields) {
        if (stringFromAttributes(sources.battery, QLatin1String(field.attribute), &text)) {
            values.insert(QLatin1String(field.key), text);
            continue;
        }
        if (!field.upowerKey)
            continue;
        const auto it = upower.constFind(QLatin1String(field.upowerKey));
        if (it == upower.constEnd())
            continue;
        const QString upowerText = it.value().toString().trimmed();
        if (!upowerText.isEmpty())
            values.insert(QLatin1String(field.key), upowerText);
    }

    if (stringFromAttributes(sources.battery, QStringLiteral("present"), &text)) {
        const QString flag = text.toLower();
        if (flag == QLatin1String("1") || flag == QLatin1String("yes")
            || flag == QLatin1String("true")) {
            values.insert(QStringLiteral("present"), true);
        } else if (flag == QLatin1String("0") || flag == QLatin1String("no")
                   || flag == QLatin1String("false")) {
            values.insert(QStringLiteral("present"), false);
        }
    } else {
        const auto it = upower.constFind(QStringLiteral("isPresent"));
        if (it != upower.constEnd() && it.value().isValid())
            values.insert(QStringLiteral("present"), it.value().toBool());
    }

    if (!snapshot.sysfsRoot.isEmpty() && !sources.battery.isEmpty()) {
        values.insert(QStringLiteral("nativePath"),
                      snapshot.sysfsRoot + QLatin1Char('/')
                              + sources.battery.first()->name);
    } else {
        const auto it = upower.constFind(QStringLiteral("nativePath"));
        if (it != upower.constEnd()) {
            const QString upowerPath = it.value().toString().trimmed();
            if (!upowerPath.isEmpty())
                values.insert(QStringLiteral("nativePath"), upowerPath);
        }
    }

    // Charger-side values may live on a non-battery supply such as usb/ac.
    int integer = 0;
    if (numberFromAttributes(sources.all, QStringLiteral("input_current_limit"), &value))
        values.insert(QStringLiteral("inputCurrentLimitA"), value / Micro);
    if (numberFromAttributes(sources.all, QStringLiteral("constant_charge_current"), &value))
        values.insert(QStringLiteral("constantChargeCurrentA"), value / Micro);
    if (numberFromAttributes(sources.all, QStringLiteral("voltage_max"), &value))
        values.insert(QStringLiteral("voltageMaxV"), value / Micro);
    if (numberFromAttributes(sources.all, QStringLiteral("charger_temp"), &value))
        values.insert(QStringLiteral("chargerTempC"), value / DeciDegree);
    if (numberFromAttributes(sources.all, QStringLiteral("system_temp_level"), &value)
        && countToInt(value, &integer)) {
        values.insert(QStringLiteral("systemTempLevel"), integer);
    }

    // Battery/BMS resistance and open-circuit voltage.
    if (numberFromAttributes(sources.battery, QStringLiteral("resistance"), &value))
        values.insert(QStringLiteral("resistance"), value / Micro);
    if (numberFromAttributes(sources.battery, QStringLiteral("resistance_id"), &value)
        && countToInt(value, &integer)) {
        values.insert(QStringLiteral("resistanceId"), integer);
    }
    if (numberFromAttributes(sources.battery, QStringLiteral("voltage_ocv"), &value))
        values.insert(QStringLiteral("voltageOcvV"), value / Micro);

    return result;
}

} // namespace Metrics
