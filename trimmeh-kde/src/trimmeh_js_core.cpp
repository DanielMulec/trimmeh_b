#include "trimmeh_js_core.h"

#include <QFile>
#include <QJSValue>
#include <QJSValueList>
#include <QStringConverter>
#include <QTextStream>

namespace {
QString readAllUtf8File(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}
} // namespace

TrimmehJsCore::TrimmehJsCore() = default;

bool TrimmehJsCore::ensureLoaded() {
    if (m_loaded) {
        return true;
    }

    const QString script = readAllUtf8File(QStringLiteral(":/trimmehCore.bundle.js"));
    if (script.isEmpty()) {
        return false;
    }

    QJSValue result = m_engine.evaluate(script, QStringLiteral("trimmehCore.bundle.js"));
    if (result.isError()) {
        return false;
    }

    const QJSValue coreObj = m_engine.globalObject().property(QStringLiteral("TrimmehCore"));
    if (!coreObj.isObject()) {
        return false;
    }

    const QJSValue trimFn = coreObj.property(QStringLiteral("trim"));
    if (!trimFn.isCallable()) {
        return false;
    }

    m_loaded = true;
    return true;
}

TrimmehTrimResult TrimmehJsCore::trim(const QString& input, const QString& aggressiveness, const TrimmehTrimOptions& options) {
    if (!ensureLoaded()) {
        return {.output = input, .changed = false, .reason = QStringLiteral("core_not_loaded")};
    }

    QJSValue coreObj = m_engine.globalObject().property(QStringLiteral("TrimmehCore"));
    QJSValue trimFn = coreObj.property(QStringLiteral("trim"));

    QJSValue optsObj = m_engine.newObject();
    optsObj.setProperty(QStringLiteral("keep_blank_lines"), options.keepBlankLines);
    optsObj.setProperty(QStringLiteral("strip_box_chars"), options.stripBoxChars);
    optsObj.setProperty(QStringLiteral("trim_prompts"), options.trimPrompts);
    optsObj.setProperty(QStringLiteral("max_lines"), options.maxLines);

    QJSValueList args;
    args << input;
    args << aggressiveness;
    args << optsObj;

    QJSValue res = trimFn.callWithInstance(coreObj, args);
    if (res.isError() || !res.isObject()) {
        return {.output = input, .changed = false, .reason = QStringLiteral("js_error")};
    }

    TrimmehTrimResult out;
    out.output = res.property(QStringLiteral("output")).toString();
    out.changed = res.property(QStringLiteral("changed")).toBool();
    out.reason = res.property(QStringLiteral("reason")).toString();

    if (out.output.isNull()) {
        out.output = input;
        out.changed = false;
    }
    return out;
}
