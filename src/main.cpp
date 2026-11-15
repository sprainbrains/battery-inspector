#include "batteryprovider.h"
#include "metrics.h"
#include "reportwriter.h"
#include "sysfsreader.h"
#include "upowersource.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickView>
#include <QTextStream>
#include <QUrl>
#include <clocale>
#include <libintl.h>

namespace {

void setApplicationIdentity()
{
    // Do not set organizationName: with both organization and application set
    // to the click id, QStandardPaths nests the app data twice.
    QCoreApplication::setApplicationName(QStringLiteral("batteryinspector.sprainbrains"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
}

void setupTranslations()
{
    setlocale(LC_ALL, "");
    bindtextdomain("batteryinspector.sprainbrains",
                   (QCoreApplication::applicationDirPath() + "/share/locale").toUtf8().constData());
    bind_textdomain_codeset("batteryinspector.sprainbrains", "UTF-8");
    textdomain("batteryinspector.sprainbrains");
}

int runDump(bool pretty)
{
    SysfsReader reader;
    const SupplySnapshot snapshot = reader.read();

    RealUpowerSource upower;
    const QVariantMap upowerData = upower.blockingRead(1000);
    const NormalizedMetrics metrics = Metrics::normalize(snapshot, upowerData);

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const QByteArray json = ReportWriter::toJson(QStringLiteral("1.0.0"),
                                                 snapshot,
                                                 metrics.values,
                                                 upowerData,
                                                 snapshot.warnings + metrics.warnings,
                                                 timestamp,
                                                 pretty);

    QTextStream stream(stdout);
    stream << json;
    if (!json.endsWith('\n'))
        stream << '\n';
    return 0;
}

void printUsage()
{
    QTextStream stream(stdout);
    stream << "Usage: batteryinspector [--dump] [--pretty] [--help]\n"
              "\n"
              "  --dump    print a JSON battery report and exit\n"
              "  --pretty  indent the JSON report\n"
              "  --help    show this help\n";
}

} // namespace

int main(int argc, char *argv[])
{
    bool dump = false;
    bool pretty = false;
    bool help = false;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == QLatin1String("--dump"))
            dump = true;
        else if (argument == QLatin1String("--pretty"))
            pretty = true;
        else if (argument == QLatin1String("--help") || argument == QLatin1String("-h"))
            help = true;
    }

    if (help) {
        printUsage();
        return 0;
    }

    if (dump || pretty) {
        QCoreApplication app(argc, argv);
        setApplicationIdentity();
        setupTranslations();
        return runDump(pretty);
    }

    QGuiApplication app(argc, argv);
    setApplicationIdentity();
    setupTranslations();

    BatteryProvider provider;
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.rootContext()->setContextProperty(QStringLiteral("battery"), &provider);
    view.setSource(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (view.status() == QQuickView::Error) {
        QTextStream(stderr) << "Failed to load QML: " << view.errors().constFirst().toString()
                            << '\n';
        return 1;
    }
    view.show();
    return app.exec();
}
