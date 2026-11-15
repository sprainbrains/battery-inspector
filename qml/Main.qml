import QtQuick 2.9
import Lomiri.Components 1.3

MainView {
    id: root
    objectName: "mainView"
    applicationName: "batteryinspector.sprainbrains"
    width: units.gu(45)
    height: units.gu(75)

    Page {
        anchors.fill: parent
        header: PageHeader {
            title: i18n.tr("Battery Inspector")
        }
        Label {
            anchors.centerIn: parent
            text: i18n.tr("Loading battery data…")
        }
    }
}
