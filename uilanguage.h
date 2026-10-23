#ifndef UILANGUAGE_H
#define UILANGUAGE_H

#include <QString>
#include <QStringList>

enum class UiLanguage {
    English,
    Chinese,
    Spanish
};

UiLanguage uiLanguageFromCode(const QString &code);
QString uiLanguageCode(UiLanguage language);
QString uiText(UiLanguage language, const char *key);
QStringList uiTranslationKeys();

#endif // UILANGUAGE_H
