#include "trimmeh_app.h"
#include "trimmeh_vectors_check.h"

#include <QApplication>

#include <cstdio>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationDomain("trimmeh.dev");
    QCoreApplication::setApplicationName("trimmeh-kde");
    QCoreApplication::setApplicationVersion("0.1.0");

    app.setQuitOnLastWindowClosed(false);

    const QStringList args = QCoreApplication::arguments();
    const int checkIndex = args.indexOf(QStringLiteral("--check-vectors"));
    if (checkIndex >= 0) {
        QString vectorsPath = QStringLiteral("tests/trim-vectors.json");
        if (checkIndex + 1 < args.size() && !args[checkIndex + 1].startsWith(QStringLiteral("--"))) {
            vectorsPath = args[checkIndex + 1];
        }

        QString err;
        const int rc = runTrimmehVectorsCheck(vectorsPath, &err);
        if (rc != 0 && !err.isEmpty()) {
            std::fprintf(stderr, "%s\n", err.toUtf8().constData());
        }
        return rc;
    }

    TrimmehApp trimmeh;
    (void)trimmeh;

    return app.exec();
}
