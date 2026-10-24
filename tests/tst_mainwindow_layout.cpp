#include <QtTest>

#include "mainwindow.h"

#include <QComboBox>
#include <QAction>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QScrollBar>

class MainWindowLayoutTest : public QObject
{
    Q_OBJECT

private slots:
    void chineseLayoutAtLargeScaleDoesNotOverlap();
    void spanishSidebarDoesNotClipCards();
    void sidebarWidthFollowsLanguageWithoutResize();
    void controlLinesAreIdleWhileDisconnected();
};

void MainWindowLayoutTest::chineseLayoutAtLargeScaleDoesNotOverlap()
{
    MainWindow window;
    window.resize(1160, 800);
    window.show();

    QComboBox *language = window.findChild<QComboBox *>(QStringLiteral("languageSelector"));
    QVERIFY(language);
    const int chinese = language->findData(QStringLiteral("zh_CN"));
    QVERIFY(chinese >= 0);
    language->setCurrentIndex(chinese);
    QCoreApplication::processEvents();

    QAction *terminalFont = window.findChild<QAction *>(QStringLiteral("actionTerminal_Font"));
    QVERIFY(terminalFont);
    QCOMPARE(terminalFont->text(), QString::fromUtf8("终端字体…"));
    QAction *terminalCacheSize = window.findChild<QAction *>(QStringLiteral("actionTerminal_Cache_Size"));
    QVERIFY(terminalCacheSize);
    QCOMPARE(terminalCacheSize->text(), QString::fromUtf8("终端缓存大小…"));
    QAction *openLogFolder = window.findChild<QAction *>(QStringLiteral("actionOpen_Log_Folder"));
    QVERIFY(openLogFolder);
    QCOMPARE(openLogFolder->text(), QString::fromUtf8("打开日志目录"));

    QScrollArea *sidebar = window.findChild<QScrollArea *>(QStringLiteral("sidebarScroll"));
    QVERIFY(sidebar);
    QVERIFY2(sidebar->horizontalScrollBar()->maximum() == 0,
             "The scaled sidebar must not need horizontal scrolling");

    const QStringList labelNames = {
        QStringLiteral("label"), QStringLiteral("label_2"), QStringLiteral("DataBits"),
        QStringLiteral("label_3"), QStringLiteral("label_4"), QStringLiteral("label_5")
    };
    const QStringList fieldNames = {
        QStringLiteral("comboBox_serialPort"), QStringLiteral("comboBox_baudrate"),
        QStringLiteral("comboBox_databits"), QStringLiteral("comboBox_stopbits"),
        QStringLiteral("comboBox_checkdigit"), QStringLiteral("comboBox_flowctrl")
    };

    for (int row = 0; row < labelNames.size(); ++row) {
        QLabel *label = window.findChild<QLabel *>(labelNames.at(row));
        QComboBox *field = window.findChild<QComboBox *>(fieldNames.at(row));
        QVERIFY(label);
        QVERIFY(field);
        const QRect labelRect(label->mapTo(&window, QPoint()), label->size());
        const QRect fieldRect(field->mapTo(&window, QPoint()), field->size());
        QVERIFY2(!labelRect.intersects(fieldRect),
                 qPrintable(labelNames.at(row) + QStringLiteral(" overlaps its field")));
        QVERIFY2(label->height() >= label->sizeHint().height(),
                 qPrintable(labelNames.at(row) + QStringLiteral(" is vertically clipped")));
        QVERIFY2(field->height() >= field->sizeHint().height(),
                 qPrintable(fieldNames.at(row) + QStringLiteral(" is vertically clipped")));
    }
}

void MainWindowLayoutTest::spanishSidebarDoesNotClipCards()
{
    MainWindow window;
    window.resize(1160, 800);
    window.show();

    QComboBox *language = window.findChild<QComboBox *>(QStringLiteral("languageSelector"));
    QVERIFY(language);
    const int spanish = language->findData(QStringLiteral("es"));
    QVERIFY(spanish >= 0);
    language->setCurrentIndex(spanish);
    QCoreApplication::processEvents();

    QScrollArea *sidebar = window.findChild<QScrollArea *>(QStringLiteral("sidebarScroll"));
    QGroupBox *connection = window.findChild<QGroupBox *>(QStringLiteral("groupBox"));
    QGroupBox *receive = window.findChild<QGroupBox *>(QStringLiteral("groupBox_2"));
    QVERIFY(sidebar);
    QVERIFY(connection);
    QVERIFY(receive);
    QCOMPARE(receive->findChild<QLabel *>(QStringLiteral("label_receiveEncoding"))->text(),
             QStringLiteral("Codificación"));

    for (QWidget *card : {static_cast<QWidget *>(connection), static_cast<QWidget *>(receive),
                          static_cast<QWidget *>(window.findChild<QGroupBox *>(QStringLiteral("groupBox_3"))),
                          static_cast<QWidget *>(window.findChild<QGroupBox *>(QStringLiteral("modemLinesCard")))}) {
        QVERIFY(card);
        const QRect cardRect(card->mapTo(sidebar->viewport(), QPoint()), card->size());
        QVERIFY2(cardRect.right() <= sidebar->viewport()->rect().right(),
                 "A translated sidebar card must not be clipped by the terminal area");
    }
}

void MainWindowLayoutTest::sidebarWidthFollowsLanguageWithoutResize()
{
    MainWindow window;
    window.resize(1160, 800);
    window.show();

    QComboBox *language = window.findChild<QComboBox *>(QStringLiteral("languageSelector"));
    QScrollArea *sidebar = window.findChild<QScrollArea *>(QStringLiteral("sidebarScroll"));
    QVERIFY(language);
    QVERIFY(sidebar);

    const int chinese = language->findData(QStringLiteral("zh_CN"));
    const int spanish = language->findData(QStringLiteral("es"));
    QVERIFY(chinese >= 0);
    QVERIFY(spanish >= 0);

    language->setCurrentIndex(chinese);
    QTRY_VERIFY(sidebar->widget()->width() > 0);
    const int chineseWidth = sidebar->widget()->width();

    language->setCurrentIndex(spanish);
    QTRY_VERIFY(sidebar->widget()->width() > chineseWidth);
    const int spanishWidth = sidebar->widget()->width();

    language->setCurrentIndex(chinese);
    QTRY_VERIFY(sidebar->widget()->width() < spanishWidth);
}

void MainWindowLayoutTest::controlLinesAreIdleWhileDisconnected()
{
    MainWindow window;
    window.show();

    QGroupBox *card = window.findChild<QGroupBox *>(QStringLiteral("modemLinesCard"));
    QVERIFY(card);
    const QList<QPushButton *> buttons = card->findChildren<QPushButton *>();
    QCOMPARE(buttons.size(), 3);
    for (QPushButton *button : buttons) {
        QVERIFY2(!button->isEnabled(), qPrintable(button->text() + QStringLiteral(" is usable without a port")));
        QVERIFY(!button->isChecked());
    }

    int indicators = 0;
    for (QLabel *label : card->findChildren<QLabel *>()) {
        if (!label->property("modemLed").toBool())
            continue;
        ++indicators;
        QCOMPARE(label->property("lineState").toString(), QStringLiteral("unknown"));
    }
    QCOMPARE(indicators, 4);
}

QTEST_MAIN(MainWindowLayoutTest)
#include "tst_mainwindow_layout.moc"
