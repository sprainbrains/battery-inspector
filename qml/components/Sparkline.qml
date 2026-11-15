import QtQuick 2.9
import Lomiri.Components 1.3

Item {
    id: root

    property var samples: []
    property var stats: ({})
    property string metric: "voltage"
    property string unit: "V"

    implicitHeight: units.gu(16)

    function hasValue(value) {
        return value !== undefined && value !== null && !isNaN(value)
    }

    function valueText(value) {
        if (!hasValue(value))
            return "—"
        return Number(value).toLocaleString(Qt.locale(), "f", 2)
    }

    Canvas {
        id: canvas

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: labels.top
            margins: units.gu(0.5)
        }

        visible: root.stats.count !== undefined && root.stats.count > 1
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            if (!root.visible || root.samples.length < 2)
                return;

            var min = root.stats.min;
            var max = root.stats.max;
            if (!root.hasValue(min) || !root.hasValue(max))
                return;

            var span = max - min;
            if (!(span > 0))
                span = 1;

            var t0 = root.samples[0].t;
            var t1 = root.samples[root.samples.length - 1].t;
            var dt = t1 - t0;
            if (!(dt > 0))
                dt = 1;

            var left = units.gu(0.5);
            var right = width - units.gu(0.5);
            var top = units.gu(0.5);
            var bottom = height - units.gu(0.5);

            ctx.strokeStyle = theme.palette.normal.base;
            ctx.lineWidth = units.dp(1);
            ctx.beginPath();
            ctx.moveTo(left, bottom);
            ctx.lineTo(right, bottom);
            ctx.stroke();

            ctx.strokeStyle = theme.palette.normal.positive;
            ctx.lineWidth = units.dp(2);
            ctx.lineJoin = "round";
            ctx.beginPath();

            var started = false;
            for (var i = 0; i < root.samples.length; ++i) {
                var sample = root.samples[i];
                var value = sample[root.metric];
                if (!root.hasValue(value)) {
                    started = false;
                    continue;
                }

                var x = left + (sample.t - t0) / dt * (right - left);
                var y = bottom - (value - min) / span * (bottom - top);

                if (!started || sample.gap)
                    ctx.moveTo(x, y);
                else
                    ctx.lineTo(x, y);
                started = true;
            }
            ctx.stroke();
        }

        Connections {
            target: root
            function onSamplesChanged() { canvas.requestPaint() }
            function onStatsChanged() { canvas.requestPaint() }
            function onMetricChanged() { canvas.requestPaint() }
        }
    }

    Row {
        id: labels
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        Label {
            width: parent.width / 3
            fontSize: "x-small"
            color: theme.palette.normal.backgroundText
            opacity: 0.7
            text: i18n.tr("min %1 %2").arg(root.valueText(root.stats.min)).arg(root.unit)
        }
        Label {
            width: parent.width / 3
            horizontalAlignment: Text.AlignHCenter
            fontSize: "x-small"
            color: theme.palette.normal.backgroundText
            opacity: 0.7
            text: i18n.tr("avg %1 %2").arg(root.valueText(root.stats.avg)).arg(root.unit)
        }
        Label {
            width: parent.width / 3
            horizontalAlignment: Text.AlignRight
            fontSize: "x-small"
            color: theme.palette.normal.backgroundText
            opacity: 0.7
            text: i18n.tr("max %1 %2").arg(root.valueText(root.stats.max)).arg(root.unit)
        }
    }

    Label {
        anchors.centerIn: parent
        visible: !canvas.visible
        text: i18n.tr("Collecting data…")
        fontSize: "small"
        color: theme.palette.normal.backgroundText
        opacity: 0.7
    }
}
