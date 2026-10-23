#include <QtTest>

#include "displaybuffer.h"

class DisplayBufferTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesTextBelowLimit();
    void dropsOldestTextAtLimit();
    void boundsSingleOversizedAppend();
};

void DisplayBufferTest::preservesTextBelowLimit()
{
    DisplayBuffer buffer(8);
    buffer.append(QStringLiteral("abc"));
    buffer.append(QStringLiteral("def"));
    QCOMPARE(buffer.take(), QStringLiteral("abcdef"));
    QVERIFY(buffer.isEmpty());
}

void DisplayBufferTest::dropsOldestTextAtLimit()
{
    DisplayBuffer buffer(8);
    buffer.append(QStringLiteral("abcdef"));
    buffer.append(QStringLiteral("WXYZ"));
    QCOMPARE(buffer.size(), 8);
    QCOMPARE(buffer.take(), QStringLiteral("cdefWXYZ"));
}

void DisplayBufferTest::boundsSingleOversizedAppend()
{
    DisplayBuffer buffer(4);
    buffer.append(QStringLiteral("0123456789"));
    QCOMPARE(buffer.take(), QStringLiteral("6789"));
}

QTEST_APPLESS_MAIN(DisplayBufferTest)
#include "tst_displaybuffer.moc"
