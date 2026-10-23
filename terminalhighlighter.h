// SPDX-License-Identifier: MPL-2.0
#ifndef TERMINALHIGHLIGHTER_H
#define TERMINALHIGHLIGHTER_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class TerminalHighlighter final : public QSyntaxHighlighter
{
public:
    explicit TerminalHighlighter(QTextDocument *document);

    void setDarkSurface(bool darkSurface);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression expression;
        QTextCharFormat format;
    };

    void rebuildRules();

    bool darkSurface_ = false;
    QList<Rule> errorRules_;
    QList<Rule> successRules_;
};

#endif // TERMINALHIGHLIGHTER_H
