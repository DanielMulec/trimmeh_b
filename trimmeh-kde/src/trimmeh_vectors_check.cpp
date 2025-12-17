#include "trimmeh_vectors_check.h"

#include "trimmeh_js_core.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
TrimmehTrimOptions optionsFromJson(const QJsonObject& obj) {
    TrimmehTrimOptions opts;
    if (obj.contains(QStringLiteral("keep_blank_lines"))) {
        opts.keepBlankLines = obj.value(QStringLiteral("keep_blank_lines")).toBool();
    }
    if (obj.contains(QStringLiteral("strip_box_chars"))) {
        opts.stripBoxChars = obj.value(QStringLiteral("strip_box_chars")).toBool();
    }
    if (obj.contains(QStringLiteral("trim_prompts"))) {
        opts.trimPrompts = obj.value(QStringLiteral("trim_prompts")).toBool();
    }
    if (obj.contains(QStringLiteral("max_lines"))) {
        opts.maxLines = obj.value(QStringLiteral("max_lines")).toInt(opts.maxLines);
    }
    return opts;
}

bool expectFieldEquals(QString* errorOut, const QString& name, const QString& field, const QString& actual, const QString& expected) {
    if (actual == expected) {
        return true;
    }
    if (errorOut) {
        *errorOut = QStringLiteral("[%1] mismatch %2: expected %3, got %4").arg(name, field, expected, actual);
    }
    return false;
}

bool expectFieldEquals(QString* errorOut, const QString& name, const QString& field, bool actual, bool expected) {
    if (actual == expected) {
        return true;
    }
    if (errorOut) {
        *errorOut =
            QStringLiteral("[%1] mismatch %2: expected %3, got %4").arg(name, field, expected ? "true" : "false", actual ? "true" : "false");
    }
    return false;
}
} // namespace

int runTrimmehVectorsCheck(const QString& vectorsPath, QString* errorOut) {
    QFile f(vectorsPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Unable to open vectors file: %1").arg(vectorsPath);
        }
        return 2;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Vectors file is not a JSON array: %1").arg(vectorsPath);
        }
        return 2;
    }

    TrimmehJsCore core;

    const QJsonArray arr = doc.array();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString(QStringLiteral("<unnamed>"));
        const QString input = o.value(QStringLiteral("input")).toString();
        const QString aggressiveness = o.value(QStringLiteral("aggressiveness")).toString(QStringLiteral("normal"));

        TrimmehTrimOptions opts;
        if (o.contains(QStringLiteral("options")) && o.value(QStringLiteral("options")).isObject()) {
            opts = optionsFromJson(o.value(QStringLiteral("options")).toObject());
        }

        const QJsonObject expected = o.value(QStringLiteral("expected")).toObject();
        const QString expectedOutput = expected.value(QStringLiteral("output")).toString();
        const bool expectedChanged = expected.value(QStringLiteral("changed")).toBool();
        const QString expectedReason = expected.value(QStringLiteral("reason")).toString();

        const auto res = core.trim(input, aggressiveness, opts);

        if (!expectFieldEquals(errorOut, name, QStringLiteral("output"), res.output, expectedOutput)) {
            return 1;
        }
        if (!expectFieldEquals(errorOut, name, QStringLiteral("changed"), res.changed, expectedChanged)) {
            return 1;
        }
        if (!expectedReason.isEmpty()) {
            if (!expectFieldEquals(errorOut, name, QStringLiteral("reason"), res.reason, expectedReason)) {
                return 1;
            }
        }
    }

    return 0;
}

