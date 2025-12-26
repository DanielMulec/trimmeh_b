#include "trim_core.h"

#include <QFile>
#include <QJSValueList>

namespace {
QString formatJsError(const QJSValue &error, const QString &sourcePath) {
    const int line = error.property(QStringLiteral("lineNumber")).toInt();
    const QString message = error.toString();
    if (!sourcePath.isEmpty()) {
        return QStringLiteral("%1:%2: %3").arg(sourcePath).arg(line).arg(message);
    }
    return QStringLiteral("line %1: %2").arg(line).arg(message);
}
}

bool TrimCore::load(const QString &jsPath, QString *errorMessage) {
    QFile file(jsPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open JS bundle: %1").arg(jsPath);
        }
        return false;
    }

    const QString script = QString::fromUtf8(file.readAll());
    file.close();

    QJSValue eval = m_engine.evaluate(script, jsPath);
    if (eval.isError()) {
        if (errorMessage) {
            *errorMessage = formatJsError(eval, jsPath);
        }
        return false;
    }

    const QJSValue core = m_engine.globalObject().property(QStringLiteral("TrimmehCore"));
    if (!core.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TrimmehCore global not found in JS bundle");
        }
        return false;
    }

    const QJSValue trimFunc = core.property(QStringLiteral("trim"));
    if (!trimFunc.isCallable()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TrimmehCore.trim is not callable");
        }
        return false;
    }

    m_trimFunc = trimFunc;
    m_ready = true;
    return true;
}

TrimResult TrimCore::trim(const QString &input,
                          const QString &aggressiveness,
                          const TrimOptions &options,
                          QString *errorMessage) {
    TrimResult result;
    result.output = input;
    result.changed = false;

    if (!m_ready) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TrimCore not initialized");
        }
        return result;
    }

    QJSValue opts = m_engine.newObject();
    opts.setProperty(QStringLiteral("keep_blank_lines"), options.keepBlankLines);
    opts.setProperty(QStringLiteral("strip_box_chars"), options.stripBoxChars);
    opts.setProperty(QStringLiteral("trim_prompts"), options.trimPrompts);
    opts.setProperty(QStringLiteral("max_lines"), options.maxLines);

    QJSValueList args;
    args << QJSValue(input) << QJSValue(aggressiveness) << opts;

    QJSValue res = m_trimFunc.call(args);
    if (res.isError()) {
        if (errorMessage) {
            *errorMessage = formatJsError(res, QString());
        }
        return result;
    }

    result.output = res.property(QStringLiteral("output")).toString();
    result.changed = res.property(QStringLiteral("changed")).toBool();
    const QJSValue reason = res.property(QStringLiteral("reason"));
    if (reason.isString()) {
        result.reason = reason.toString();
    }
    return result;
}
