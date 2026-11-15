#include <QGuiApplication>
#include <QCoreApplication>
#include <QQuickView>
#include <QUrl>
#include <clocale>
#include <libintl.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("batteryinspector.sprainbrains"));
    QCoreApplication::setApplicationName(QStringLiteral("batteryinspector.sprainbrains"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    setlocale(LC_ALL, "");
    bindtextdomain("batteryinspector.sprainbrains",
                   (QCoreApplication::applicationDirPath() + "/share/locale").toUtf8().constData());
    bind_textdomain_codeset("batteryinspector.sprainbrains", "UTF-8");
    textdomain("batteryinspector.sprainbrains");

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:/Main.qml"));
    view.show();
    return app.exec();
}
