// SPDX-License-Identifier: MPL-2.0

#include "terminalhighlighter.h"

#include <QColor>
#include <QTextDocument>

namespace {

QTextCharFormat statusFormat(const QColor &color)
{
    QTextCharFormat format;
    format.setForeground(color);
    format.setFontWeight(QFont::DemiBold);
    return format;
}

} // namespace

TerminalHighlighter::TerminalHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
    rebuildRules();
}

void TerminalHighlighter::setDarkSurface(bool darkSurface)
{
    if (darkSurface_ == darkSurface)
        return;
    darkSurface_ = darkSurface;
    rebuildRules();
    rehighlight();
}

void TerminalHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &rule : errorRules_) {
        auto match = rule.expression.globalMatch(text);
        while (match.hasNext()) {
            const QRegularExpressionMatch current = match.next();
            setFormat(current.capturedStart(), current.capturedLength(), rule.format);
        }
    }
    for (const Rule &rule : successRules_) {
        auto match = rule.expression.globalMatch(text);
        while (match.hasNext()) {
            const QRegularExpressionMatch current = match.next();
            setFormat(current.capturedStart(), current.capturedLength(), rule.format);
        }
    }
}

void TerminalHighlighter::rebuildRules()
{
    const QTextCharFormat error = statusFormat(darkSurface_ ? QColor("#ff7b8f") : QColor("#b42318"));
    const QTextCharFormat success = statusFormat(darkSurface_ ? QColor("#79dca7") : QColor("#087443"));
    const auto makeRule = [](const QString &pattern, const QTextCharFormat &format) {
        return Rule{QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption), format};
    };

    errorRules_ = {
        makeRule(QStringLiteral(R"(\b(?:fail(?:ed|ure)?|error|fatal|panic|abort(?:ed)?|denied|invalid|timeout|exception|critical|ko|ng)\b)"), error),
        makeRule(QStringLiteral("失败|错误|致命|超时|拒绝|异常"), error),
        makeRule(QStringLiteral(R"(\b(?:fall[oa]|fallida|fatal|denegado|excepci[oó]n)\b|tiempo de espera)"), error),
    };
    successRules_ = {
        makeRule(QStringLiteral(R"(\b(?:ok|pass(?:ed)?|success(?:ful(?:ly)?)?|done|complete(?:d)?|ready|connected|accepted|valid|good)\b)"), success),
        makeRule(QStringLiteral("成功|完成|就绪|已连接|通过"), success),
        makeRule(QStringLiteral(R"(\b(?:[ée]xito|correcto|completado|listo|conectado|aceptado|v[aá]lido)\b)"), success),
    };
}
