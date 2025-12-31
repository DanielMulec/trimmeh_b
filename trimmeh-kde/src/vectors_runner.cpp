#include "trim_core.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {
QString defaultCorePath() {
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("trimmeh-core.js"));
}

QString defaultVectorsPath() {
    const QString appDir = QCoreApplication::applicationDirPath();
    return QDir::cleanPath(QDir(appDir).filePath(QStringLiteral("../tests/trim-vectors.json")));
}

QString requiredString(const QJsonObject &obj, const QString &key, bool *ok) {
    if (!obj.contains(key) || !obj.value(key).isString()) {
        if (ok) *ok = false;
        return QString();
    }
    return obj.value(key).toString();
}

bool getBool(const QJsonObject &obj, const QString &key, bool fallback) {
    const QJsonValue value = obj.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

int getInt(const QJsonObject &obj, const QString &key, int fallback) {
    const QJsonValue value = obj.value(key);
    return value.isDouble() ? value.toInt() : fallback;
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("trimmeh-kde-vectors"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Trimmeh KDE vector test runner"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption coreOpt(QStringList() << QStringLiteral("c") << QStringLiteral("core"),
                               QStringLiteral("Path to trimmeh-core.js"),
                               QStringLiteral("path"));
    QCommandLineOption vectorsOpt(QStringList() << QStringLiteral("t") << QStringLiteral("vectors"),
                                  QStringLiteral("Path to trim-vectors.json"),
                                  QStringLiteral("path"));
    parser.addOption(coreOpt);
    parser.addOption(vectorsOpt);
    parser.process(app);

    const QString corePath = parser.value(coreOpt).isEmpty()
        ? defaultCorePath()
        : parser.value(coreOpt);
    const QString vectorsPath = parser.value(vectorsOpt).isEmpty()
        ? defaultVectorsPath()
        : parser.value(vectorsOpt);

    QTextStream out(stdout);
    QTextStream err(stderr);

    TrimCore core;
    QString loadError;
    if (!core.load(corePath, &loadError)) {
        err << "Failed to load core JS: " << loadError << "\n";
        err << "Path: " << corePath << "\n";
        return 2;
    }

    QFile file(vectorsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        err << "Failed to open vectors: " << vectorsPath << "\n";
        return 3;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (doc.isNull() || parseError.error != QJsonParseError::NoError) {
        err << "Failed to parse vectors JSON: " << parseError.errorString() << "\n";
        return 4;
    }
    if (!doc.isArray()) {
        err << "Vectors JSON is not an array\n";
        return 5;
    }

    const QJsonArray cases = doc.array();
    int total = 0;
    int failures = 0;

    for (int i = 0; i < cases.size(); ++i) {
        const QJsonValue value = cases.at(i);
        if (!value.isObject()) {
            err << "Case " << i << " is not an object\n";
            failures += 1;
            continue;
        }
        const QJsonObject obj = value.toObject();
        bool ok = true;
        const QString name = obj.value(QStringLiteral("name")).toString(QStringLiteral("case_%1").arg(i));
        const QString input = requiredString(obj, QStringLiteral("input"), &ok);
        const QString aggressiveness = obj.value(QStringLiteral("aggressiveness"))
            .toString(QStringLiteral("normal"));
        if (!ok) {
            err << name << ": missing required fields\n";
            failures += 1;
            continue;
        }

        TrimOptions options;
        if (obj.contains(QStringLiteral("options")) && obj.value(QStringLiteral("options")).isObject()) {
            const QJsonObject opts = obj.value(QStringLiteral("options")).toObject();
            options.keepBlankLines = getBool(opts, QStringLiteral("keep_blank_lines"), options.keepBlankLines);
            options.stripBoxChars = getBool(opts, QStringLiteral("strip_box_chars"), options.stripBoxChars);
            options.trimPrompts = getBool(opts, QStringLiteral("trim_prompts"), options.trimPrompts);
            options.maxLines = getInt(opts, QStringLiteral("max_lines"), options.maxLines);
        }

        if (!obj.contains(QStringLiteral("expected")) || !obj.value(QStringLiteral("expected")).isObject()) {
            err << name << ": missing expected\n";
            failures += 1;
            continue;
        }
        const QJsonObject expected = obj.value(QStringLiteral("expected")).toObject();
        const QString expectedOutput = expected.value(QStringLiteral("output")).toString();
        const bool expectedChanged = expected.value(QStringLiteral("changed")).toBool();
        const QString expectedReason = expected.value(QStringLiteral("reason")).toString();

        QString error;
        const TrimResult result = core.trim(input, aggressiveness, options, &error);
        if (!error.isEmpty()) {
            err << name << ": trim error: " << error << "\n";
            failures += 1;
            continue;
        }

        bool pass = true;
        if (result.output != expectedOutput) {
            err << name << ": output mismatch\n";
            err << "  expected: " << expectedOutput << "\n";
            err << "  actual:   " << result.output << "\n";
            pass = false;
        }
        if (result.changed != expectedChanged) {
            err << name << ": changed mismatch (expected "
                << (expectedChanged ? "true" : "false")
                << ", got " << (result.changed ? "true" : "false") << ")\n";
            pass = false;
        }
        if (!expectedReason.isEmpty() && result.reason != expectedReason) {
            err << name << ": reason mismatch (expected "
                << expectedReason << ", got " << result.reason << ")\n";
            pass = false;
        }

        total += 1;
        if (!pass) {
            failures += 1;
        }
    }

    const int passed = total - failures;
    out << "Vectors: " << passed << " passed, " << failures << " failed.\n";
    return failures == 0 ? 0 : 1;
}
