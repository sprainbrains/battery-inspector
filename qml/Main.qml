import QtQuick 2.9
import QtQuick.Layouts 1.3
import Lomiri.Components 1.3
import Lomiri.Components.Popups 1.3
import Qt.labs.settings 1.0
import "components"

MainView {
    id: root
    objectName: "mainView"
    applicationName: "batteryinspector.sprainbrains"
    automaticOrientation: true

    width: units.gu(45)
    height: units.gu(75)

    property var summary: battery.summary
    property bool ready: false

    Settings {
        id: settings
        category: "batteryinspector"
        property int pollInterval: 2000
        property string graphMetric: "voltage"
    }

    Component.onCompleted: {
        battery.pollInterval = settings.pollInterval
        battery.historyMetric = settings.graphMetric
        battery.start()
        ready = true
    }
    Component.onDestruction: battery.stop()

    function hasValue(value) {
        return value !== undefined && value !== null && !isNaN(value)
    }

    function formatNumber(value, digits) {
        if (!hasValue(value))
            return "—"
        return Number(value).toLocaleString(Qt.locale(), "f", digits)
    }

    function formatPercent(value) {
        if (!hasValue(value))
            return "—"
        return formatNumber(value, 1) + "%"
    }

    function formatWithUnit(value, digits, unit) {
        if (!hasValue(value))
            return "—"
        return formatNumber(value, digits) + " " + unit
    }

    function formatDuration(seconds) {
        if (!hasValue(seconds) || seconds <= 0)
            return "—"
        var hours = Math.floor(seconds / 3600)
        var minutes = Math.round((seconds % 3600) / 60)
        return hours > 0 ? (hours + " " + i18n.tr("h") + " " + minutes + " " + i18n.tr("min"))
                         : (minutes + " " + i18n.tr("min"))
    }

    function statusText(key) {
        if (key === "charging")
            return i18n.tr("Charging")
        if (key === "discharging")
            return i18n.tr("Discharging")
        if (key === "full")
            return i18n.tr("Full")
        if (key === "notCharging")
            return i18n.tr("Not charging")
        return i18n.tr("Unknown")
    }

    function timeText() {
        if (summary.timeToEmptySec !== undefined)
            return i18n.tr("Time to empty: %1").arg(formatDuration(summary.timeToEmptySec))
        if (summary.timeToFullSec !== undefined)
            return i18n.tr("Time to full: %1").arg(formatDuration(summary.timeToFullSec))
        return ""
    }

    function presentText(value) {
        if (value === undefined || value === null)
            return "—"
        return value ? i18n.tr("Yes") : i18n.tr("No")
    }

    function sourceText(code) {
        if (code === "ok")
            return i18n.tr("OK")
        if (code === "no-battery")
            return i18n.tr("No battery")
        return i18n.tr("Unavailable")
    }

    function metricUnit() {
        if (battery.historyMetric === "voltage")
            return "V"
        if (battery.historyMetric === "current")
            return "A"
        if (battery.historyMetric === "power")
            return "W"
        return "%"
    }

    Page {
        id: page
        anchors.fill: parent

        header: PageHeader {
            title: i18n.tr("Battery Inspector")
            trailingActionBar.actions: [
                Action {
                    iconName: "view-refresh"
                    text: i18n.tr("Refresh")
                    onTriggered: battery.refresh()
                },
                Action {
                    iconName: "info"
                    text: i18n.tr("About")
                    onTriggered: PopupUtils.open(aboutDialog)
                }
            ]
        }

        Flickable {
            id: flickable
            anchors {
                top: page.header.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            contentWidth: width
            contentHeight: content.y + content.height + units.gu(2)
            clip: true
            boundsBehavior: Flickable.DragAndOvershootBounds

            Column {
                id: content
                width: Math.min(flickable.width - units.gu(2), units.gu(60))
                anchors.horizontalCenter: parent.horizontalCenter
                y: units.gu(1.5)
                spacing: units.gu(1.5)

                HeroCard {
                    width: parent.width
                    percentage: summary.percentage !== undefined ? summary.percentage : 0
                    statusKey: summary.statusKey || "unknown"
                    statusLabel: root.statusText(summary.statusKey)
                    timeText: root.timeText()
                }

                Row {
                    width: parent.width
                    spacing: units.gu(1)

                    MetricTile {
                        width: (parent.width - 3 * parent.spacing) / 4
                        label: i18n.tr("Voltage")
                        value: root.formatNumber(summary.voltage, 2)
                        unit: "V"
                    }
                    MetricTile {
                        width: (parent.width - 3 * parent.spacing) / 4
                        label: i18n.tr("Current")
                        value: summary.current !== undefined ? root.formatNumber(Math.abs(summary.current), 2) : "—"
                        unit: "A"
                    }
                    MetricTile {
                        width: (parent.width - 3 * parent.spacing) / 4
                        label: i18n.tr("Power")
                        value: summary.power !== undefined ? root.formatNumber(Math.abs(summary.power), 2) : "—"
                        unit: "W"
                    }
                    MetricTile {
                        width: (parent.width - 3 * parent.spacing) / 4
                        label: i18n.tr("Temperature")
                        value: root.formatNumber(summary.temperature, 1)
                        unit: "°C"
                    }
                }

                SectionCard {
                    title: i18n.tr("Health")

                    InfoRow {
                        label: i18n.tr("Health")
                        value: root.formatPercent(summary.healthPercent)
                    }
                    InfoRow {
                        label: i18n.tr("Wear")
                        value: root.formatPercent(summary.wearPercent)
                    }
                    InfoRow {
                        label: i18n.tr("Cycles")
                        value: summary.cycleCount !== undefined ? summary.cycleCount.toString() : "—"
                    }
                    Label {
                        width: parent.width
                        visible: summary.cycleCount === 0
                        text: i18n.tr("(0 may mean the controller does not report a cycle counter.)")
                        fontSize: "x-small"
                        wrapMode: Text.WordWrap
                        color: theme.palette.normal.backgroundText
                        opacity: 0.7
                    }
                    InfoRow {
                        label: i18n.tr("Full charge capacity")
                        value: root.formatWithUnit(summary.chargeFullAh, 3, "Ah")
                    }
                    InfoRow {
                        label: i18n.tr("Design capacity")
                        value: root.formatWithUnit(summary.chargeFullDesignAh, 3, "Ah")
                    }
                    InfoRow {
                        label: i18n.tr("Full energy")
                        value: root.formatWithUnit(summary.energyFullWh, 2, "Wh")
                        visible: summary.energyFullWh !== undefined
                    }
                    InfoRow {
                        label: i18n.tr("Design energy")
                        value: root.formatWithUnit(summary.energyFullDesignWh, 2, "Wh")
                        visible: summary.energyFullDesignWh !== undefined
                    }
                    InfoRow {
                        label: i18n.tr("Technology")
                        value: summary.technology || "—"
                    }
                    InfoRow {
                        label: i18n.tr("Kernel health")
                        value: summary.health || "—"
                        visible: summary.health !== undefined
                    }
                }

                SectionCard {
                    title: i18n.tr("Charging")

                    InfoRow {
                        label: i18n.tr("Status")
                        value: root.statusText(summary.statusKey)
                    }
                    InfoRow {
                        label: i18n.tr("Charge type")
                        value: summary.chargeType || "—"
                        visible: summary.chargeType !== undefined
                    }
                    InfoRow {
                        label: i18n.tr("Charge now")
                        value: root.formatWithUnit(summary.chargeNowAh, 3, "Ah")
                        visible: summary.chargeNowAh !== undefined
                    }
                    InfoRow {
                        label: i18n.tr("Charge counter")
                        value: root.formatWithUnit(summary.chargeCounterAh, 3, "Ah")
                        visible: summary.chargeCounterAh !== undefined
                    }
                    InfoRow {
                        label: i18n.tr("Input current limit")
                        value: root.formatWithUnit(summary.inputCurrentLimitA, 2, "A")
                        visible: summary.inputCurrentLimitA !== undefined
                    }
                    InfoRow {
                        label: i18n.tr("Constant charge current")
                        value: root.formatWithUnit(summary.constantChargeCurrentA, 2, "A")
                        visible: summary.constantChargeCurrentA !== undefined
                    }
                    InfoRow {
                        label: i18n.tr("Max voltage")
                        value: root.formatWithUnit(summary.voltageMaxV, 2, "V")
                        visible: summary.voltageMaxV !== undefined
                    }
                    InfoRow {
                        label: i18n.tr("Charger temperature")
                        value: root.formatWithUnit(summary.chargerTempC, 1, "°C")
                        visible: summary.chargerTempC !== undefined
                    }
                }

                SectionCard {
                    id: powerCard
                    title: i18n.tr("Power")
                    property var connectedSupplies: battery.supplies.filter(function(item) {
                        return item.type !== "Battery" && item.type !== "BMS"
                               && (item.online || item.present)
                    })
                    visible: connectedSupplies.length > 0

                    Repeater {
                        model: powerCard.connectedSupplies

                        InfoRow {
                            label: modelData.name + " (" + modelData.type + ")"
                            value: modelData.online
                                   ? i18n.tr("Online")
                                   : (modelData.present ? i18n.tr("Present, not online")
                                                        : i18n.tr("Offline"))
                        }
                    }
                }

                SectionCard {
                    title: i18n.tr("Device")

                    InfoRow {
                        label: i18n.tr("Model")
                        value: summary.modelName || "—"
                    }
                    InfoRow {
                        label: i18n.tr("Manufacturer")
                        value: summary.manufacturer || "—"
                    }
                    InfoRow {
                        label: i18n.tr("Serial")
                        value: summary.serialNumber || "—"
                        monospace: true
                    }
                    InfoRow {
                        label: i18n.tr("Battery type")
                        value: summary.batteryType || "—"
                    }
                    InfoRow {
                        label: i18n.tr("Native path")
                        value: summary.nativePath || "—"
                        monospace: true
                    }
                    InfoRow {
                        label: i18n.tr("Present")
                        value: root.presentText(summary.present)
                    }
                }

                SectionCard {
                    title: i18n.tr("Live (last 5 minutes)")

                    Sparkline {
                        width: parent.width
                        samples: battery.history
                        stats: battery.historyStats
                        metric: battery.historyMetric
                        unit: root.metricUnit()
                    }

                    OptionSelector {
                        width: parent.width
                        model: [i18n.tr("Voltage"), i18n.tr("Current"),
                                i18n.tr("Power"), i18n.tr("Charge")]
                        selectedIndex: {
                            var metrics = ["voltage", "current", "power", "percentage"]
                            return Math.max(0, metrics.indexOf(battery.historyMetric))
                        }
                        delegate: OptionSelectorDelegate {
                            text: modelData
                        }
                        onSelectedIndexChanged: {
                            if (!root.ready)
                                return
                            var metrics = ["voltage", "current", "power", "percentage"]
                            battery.historyMetric = metrics[selectedIndex]
                            settings.graphMetric = battery.historyMetric
                        }
                    }
                }

                SectionCard {
                    id: rawCard
                    title: i18n.tr("Raw data")
                    property bool expanded: false

                    Button {
                        width: parent.width
                        text: rawCard.expanded ? i18n.tr("Hide raw data")
                                               : i18n.tr("Show raw data")
                        onClicked: rawCard.expanded = !rawCard.expanded
                    }

                    Loader {
                        id: rawLoader
                        width: parent.width
                        active: rawCard.expanded
                        height: active && item ? item.implicitHeight : 0
                        sourceComponent: Component {
                            RawTable {
                                width: rawLoader.width
                                rows: battery.rawAttributes
                            }
                        }
                    }
                }

                SectionCard {
                    title: i18n.tr("Settings")

                    InfoRow {
                        label: i18n.tr("Refresh interval")
                        value: i18n.tr("%1 s").arg(battery.pollInterval / 1000)
                    }

                    OptionSelector {
                        width: parent.width
                        model: [1, 2, 5, 10]
                        selectedIndex: {
                            var intervals = [1, 2, 5, 10]
                            return Math.max(0, intervals.indexOf(battery.pollInterval / 1000))
                        }
                        delegate: OptionSelectorDelegate {
                            text: modelData + " " + i18n.tr("s")
                        }
                        onSelectedIndexChanged: {
                            if (!root.ready)
                                return
                            var intervals = [1, 2, 5, 10]
                            battery.pollInterval = intervals[selectedIndex] * 1000
                            settings.pollInterval = battery.pollInterval
                        }
                    }
                }

                SectionCard {
                    title: i18n.tr("Status")
                    visible: battery.sysfsStatus !== "ok"

                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: theme.palette.normal.backgroundText
                        text: battery.sysfsStatus === "no-battery"
                              ? i18n.tr("No battery found in sysfs. Showing UPower data if available.")
                              : i18n.tr("No access to battery sysfs. Showing UPower data if available.")
                    }
                }

                Column {
                    width: parent.width
                    spacing: units.gu(0.3)

                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        fontSize: "small"
                        color: theme.palette.normal.backgroundText
                        opacity: 0.7
                        text: battery.lastUpdatedMs > 0
                              ? i18n.tr("Updated: %1").arg(Qt.formatTime(new Date(battery.lastUpdatedMs), "hh:mm:ss"))
                              : i18n.tr("Waiting for data…")
                    }
                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        fontSize: "small"
                        color: theme.palette.normal.backgroundText
                        opacity: 0.7
                        text: i18n.tr("sysfs: %1 · UPower: %2")
                              .arg(root.sourceText(battery.sysfsStatus))
                              .arg(root.sourceText(battery.upowerStatus))
                    }
                }
            }
        }
    }

    Component {
        id: aboutDialog

        Dialog {
            id: dialog
            title: i18n.tr("About Battery Inspector")
            text: i18n.tr("Battery Inspector 1.0.0\n\nData sources: Linux power_supply and UPower (optional).\nNo data leaves the device.")

            TextEdit {
                id: reportClipboard
                visible: false
                text: ""
            }

            Label {
                id: savedLabel
                width: parent.width
                wrapMode: Text.WordWrap
                fontSize: "small"
                color: theme.palette.normal.backgroundText
                text: ""
            }

            Button {
                text: i18n.tr("Copy report")
                onClicked: {
                    reportClipboard.text = battery.reportJson(true)
                    reportClipboard.selectAll()
                    reportClipboard.copy()
                    savedLabel.text = i18n.tr("Report copied to clipboard")
                }
            }

            Button {
                text: i18n.tr("Save report")
                onClicked: {
                    var path = battery.saveReport()
                    savedLabel.text = path.length > 0
                            ? i18n.tr("Saved to %1").arg(path)
                            : i18n.tr("Could not save the report")
                }
            }

            Button {
                text: i18n.tr("Close")
                onClicked: PopupUtils.close(dialog)
            }
        }
    }
}
