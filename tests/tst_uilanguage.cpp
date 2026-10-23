#include "uilanguage.h"

#include <QtTest>

class UiLanguageTest : public QObject
{
    Q_OBJECT

private slots:
    void detectsSupportedLocales();
    void translatesCoreControls();
    void nonChineseLanguagesContainNoChineseText();
};

void UiLanguageTest::detectsSupportedLocales()
{
    QCOMPARE(uiLanguageFromCode(QStringLiteral("en_US")), UiLanguage::English);
    QCOMPARE(uiLanguageFromCode(QStringLiteral("zh_CN")), UiLanguage::Chinese);
    QCOMPARE(uiLanguageFromCode(QStringLiteral("es_ES")), UiLanguage::Spanish);
}

void UiLanguageTest::nonChineseLanguagesContainNoChineseText()
{
    const QRegularExpression hanCharacters(QStringLiteral("[\\x{3400}-\\x{9fff}]") );
    for (const QString &key : uiTranslationKeys()) {
        const QByteArray keyBytes = key.toLatin1();
        QVERIFY2(!uiText(UiLanguage::English, keyBytes.constData()).contains(hanCharacters),
                 qPrintable(QStringLiteral("English translation contains Chinese: %1").arg(key)));
        QVERIFY2(!uiText(UiLanguage::Spanish, keyBytes.constData()).contains(hanCharacters),
                 qPrintable(QStringLiteral("Spanish translation contains Chinese: %1").arg(key)));
    }
}

void UiLanguageTest::translatesCoreControls()
{
    QCOMPARE(uiText(UiLanguage::English, "connect"), QStringLiteral("Connect"));
    QCOMPARE(uiText(UiLanguage::Chinese, "connect"), QString::fromUtf8("连接"));
    QCOMPARE(uiText(UiLanguage::Spanish, "connect"), QStringLiteral("Conectar"));
    QCOMPARE(uiText(UiLanguage::Chinese, "terminal"), QString::fromUtf8("终端"));
    QCOMPARE(uiText(UiLanguage::Spanish, "save_log"), QStringLiteral("Guardar registro"));
}

QTEST_MAIN(UiLanguageTest)
#include "tst_uilanguage.moc"
