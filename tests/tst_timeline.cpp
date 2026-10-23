// SPDX-License-Identifier: MPL-2.0

#include <QtTest>

#include "timeline.h"

namespace {

TimelineEvent makeEvent(qint64 timestampMs, TimelineKind kind, const QByteArray &data = QByteArray(),
                        const QString &message = QString())
{
    TimelineEvent event;
    event.timestampMs = timestampMs;
    event.kind = kind;
    event.source = kind == TimelineKind::Rx ? TimelineSource::Device
        : kind == TimelineKind::Tx ? TimelineSource::Manual : TimelineSource::App;
    event.data = data;
    event.message = message;
    return event;
}

} // namespace

class TimelineTest : public QObject
{
    Q_OBJECT

private slots:
    void splitsChunksIntoLines();
    void keepsFirstByteTimestampForSplitLines();
    void flushesPartialLine();
    void capsLongLines();
    void escapesContent();
    void tracksDeltasAndCounts();
    void trimsOldestEvents();
    void measuresResponseToTx();
    void filtersByKindAndText();
    void writesCsv();
    void roundTripsJsonLines();
    void rejectsMalformedJsonLines();
};

void TimelineTest::splitsChunksIntoLines()
{
    TimelineLineAssembler assembler;
    const QVector<TimelineEvent> lines = assembler.append("one\r\ntwo\nthr", 100);
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0).data, QByteArray("one\r\n"));
    QCOMPARE(lines.at(1).data, QByteArray("two\n"));
    QCOMPARE(lines.at(0).kind, TimelineKind::Rx);
    QCOMPARE(lines.at(0).source, TimelineSource::Device);
    QVERIFY(assembler.hasPending());
}

void TimelineTest::keepsFirstByteTimestampForSplitLines()
{
    TimelineLineAssembler assembler;
    QVERIFY(assembler.append("hea", 100).isEmpty());
    const QVector<TimelineEvent> lines = assembler.append("rtbeat\nnext", 140);
    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.at(0).data, QByteArray("heartbeat\n"));
    QCOMPARE(lines.at(0).timestampMs, qint64(100));

    const QVector<TimelineEvent> rest = assembler.flush();
    QCOMPARE(rest.size(), 1);
    QCOMPARE(rest.at(0).timestampMs, qint64(140));
}

void TimelineTest::flushesPartialLine()
{
    TimelineLineAssembler assembler;
    assembler.append("prompt> ", 10);
    const QVector<TimelineEvent> lines = assembler.flush();
    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.at(0).data, QByteArray("prompt> "));
    QVERIFY(!assembler.hasPending());
    QVERIFY(assembler.flush().isEmpty());
}

void TimelineTest::capsLongLines()
{
    TimelineLineAssembler assembler(4);
    const QVector<TimelineEvent> lines = assembler.append("abcdefghij", 1);
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0).data, QByteArray("abcd"));
    QCOMPARE(lines.at(1).data, QByteArray("efgh"));
    QCOMPARE(assembler.flush().at(0).data, QByteArray("ij"));
}

void TimelineTest::escapesContent()
{
    const TimelineEvent line = makeEvent(0, TimelineKind::Rx, QByteArray("a\tb\x01\r\n"));
    QCOMPARE(timelineContentText(line, false), QStringLiteral("a\\tb\\x01"));
    QCOMPARE(timelineContentText(line, true), QStringLiteral("61 09 62 01 0D 0A"));

    const TimelineEvent utf8 = makeEvent(0, TimelineKind::Rx, QByteArray("\xe4\xbd\xa0\xe5\xa5\xbd"));
    QCOMPARE(timelineContentText(utf8, false), QString::fromUtf8("你好"));

    const TimelineEvent binary = makeEvent(0, TimelineKind::Rx, QByteArray("\xff" "A", 2));
    QCOMPARE(timelineContentText(binary, false), QStringLiteral("\\xffA"));

    const TimelineEvent system = makeEvent(0, TimelineKind::System, QByteArray(), QStringLiteral("Connected"));
    QCOMPARE(timelineContentText(system, true), QStringLiteral("Connected"));
}

void TimelineTest::tracksDeltasAndCounts()
{
    TimelineModel model;
    model.append({makeEvent(1000, TimelineKind::System, QByteArray(), QStringLiteral("Connected")),
                  makeEvent(1250, TimelineKind::Tx, "ping"),
                  makeEvent(3500, TimelineKind::Rx, "pong\n")});

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.index(0, TimelineModel::DeltaColumn).data().toString(), QString::fromUtf8("—"));
    QCOMPARE(model.index(1, TimelineModel::DeltaColumn).data().toString(), QStringLiteral("+250 ms"));
    QCOMPARE(model.index(2, TimelineModel::DeltaColumn).data().toString(), QStringLiteral("+2.250 s"));
    QCOMPARE(model.index(1, TimelineModel::TypeColumn).data().toString(), QStringLiteral("TX"));
    QCOMPARE(model.index(2, TimelineModel::BytesColumn).data().toInt(), 5);
    QCOMPARE(model.index(2, TimelineModel::ContentColumn).data().toString(), QStringLiteral("pong"));
    QCOMPARE(model.count(TimelineKind::Rx), 1);
    QCOMPARE(model.count(TimelineKind::Tx), 1);
    QCOMPARE(model.count(TimelineKind::System), 1);

    model.setHexContent(true);
    QCOMPARE(model.index(1, TimelineModel::ContentColumn).data().toString(), QStringLiteral("70 69 6E 67"));
}

void TimelineTest::trimsOldestEvents()
{
    TimelineModel model;
    model.setMaxEvents(100);
    QVector<TimelineEvent> batch;
    for (int i = 0; i < 101; ++i)
        batch.append(makeEvent(i, i % 2 ? TimelineKind::Tx : TimelineKind::Rx, "x"));
    model.append(batch);

    // Trimming drops 5% beyond the limit at once.
    QCOMPARE(model.rowCount(), 95);
    QCOMPARE(model.eventAt(0).timestampMs, qint64(6));
    QCOMPARE(model.count(TimelineKind::Rx) + model.count(TimelineKind::Tx), 95);
}

void TimelineTest::measuresResponseToTx()
{
    TimelineModel model;
    model.append({makeEvent(100, TimelineKind::Tx, "get\n"),
                  makeEvent(127, TimelineKind::Rx, "ok\n"),
                  makeEvent(130, TimelineKind::System, QByteArray(), QStringLiteral("note")),
                  makeEvent(140, TimelineKind::Rx, "done\n"),
                  makeEvent(200, TimelineKind::Tx, "get\n"),
                  makeEvent(260, TimelineKind::Rx, "late\n")});

    const TimelineModel::Response response = model.responseTo(0);
    QCOMPARE(response.firstLatencyMs, qint64(27));
    QCOMPARE(response.rxEvents, 2);
    QCOMPARE(response.rxBytes, qint64(8));

    model.setResponseAnchor(0);
    QVERIFY(model.index(1, 0).data(Qt::BackgroundRole).isValid());
    QVERIFY(!model.index(2, 0).data(Qt::BackgroundRole).isValid());
    QVERIFY(model.index(3, 0).data(Qt::BackgroundRole).isValid());
    QVERIFY(!model.index(5, 0).data(Qt::BackgroundRole).isValid());

    // Without a TX after the anchor, newly appended RX still counts as the response.
    model.setResponseAnchor(4);
    model.append({makeEvent(300, TimelineKind::Rx, "more\n")});
    QVERIFY(model.index(6, 0).data(Qt::BackgroundRole).isValid());
    QCOMPARE(model.responseTo(4).rxEvents, 2);
    QCOMPARE(model.responseTo(4).firstLatencyMs, qint64(60));
}

void TimelineTest::filtersByKindAndText()
{
    TimelineModel model;
    model.append({makeEvent(1, TimelineKind::Rx, "heartbeat 1\n"),
                  makeEvent(2, TimelineKind::Tx, "reset\n"),
                  makeEvent(3, TimelineKind::Rx, "Heartbeat 2\n"),
                  makeEvent(4, TimelineKind::System, QByteArray(), QStringLiteral("Disconnected"))});
    TimelineFilterModel filter;
    filter.setSourceModel(&model);
    QCOMPARE(filter.rowCount(), 4);

    filter.setKindVisible(TimelineKind::Rx, false);
    QCOMPARE(filter.rowCount(), 2);

    filter.setKindVisible(TimelineKind::Rx, true);
    filter.setSearchText(QStringLiteral("HEARTBEAT"));
    QCOMPARE(filter.rowCount(), 2);

    filter.setSearchText(QStringLiteral("disconn"));
    QCOMPARE(filter.rowCount(), 1);
}

void TimelineTest::writesCsv()
{
    const QDateTime start(QDate(2026, 10, 23), QTime(15, 36, 1, 120));
    const qint64 startMs = start.toMSecsSinceEpoch();
    const QByteArray csv = TimelineFile::toCsv({
        makeEvent(startMs, TimelineKind::Tx, "a,\"b\""),
        makeEvent(startMs + 27, TimelineKind::Rx, "ok\r\n")});
    QVERIFY(csv.startsWith("\xEF\xBB\xBF"));
    const QStringList lines = QString::fromUtf8(csv.mid(3)).split(QStringLiteral("\r\n"));

    QCOMPARE(lines.at(0), QStringLiteral("time,delta_ms,type,source,bytes,text,hex"));
    QCOMPARE(lines.at(1), QStringLiteral("2026-10-23 15:36:01.120,,tx,manual,5,\"a,\"\"b\"\"\",61 2C 22 62 22"));
    QCOMPARE(lines.at(2), QStringLiteral("2026-10-23 15:36:01.147,27,rx,device,4,ok,6F 6B 0D 0A"));
}

void TimelineTest::roundTripsJsonLines()
{
    const QVector<TimelineEvent> original = {
        makeEvent(1761205000000, TimelineKind::System, QByteArray(), QString::fromUtf8("已连接 ttyACM0")),
        makeEvent(1761205000500, TimelineKind::Tx, QByteArray("\x00\xff\n", 3)),
        makeEvent(1761205000527, TimelineKind::Rx, "ok\n")};

    QVector<TimelineEvent> loaded;
    QVERIFY(TimelineFile::fromJsonLines(TimelineFile::toJsonLines(original), &loaded));
    QCOMPARE(loaded.size(), original.size());
    for (int i = 0; i < original.size(); ++i) {
        QCOMPARE(loaded.at(i).timestampMs, original.at(i).timestampMs);
        QCOMPARE(loaded.at(i).kind, original.at(i).kind);
        QCOMPARE(loaded.at(i).source, original.at(i).source);
        QCOMPARE(loaded.at(i).data, original.at(i).data);
        QCOMPARE(loaded.at(i).message, original.at(i).message);
    }
}

void TimelineTest::rejectsMalformedJsonLines()
{
    const QByteArray good = "{\"time_ms\":1,\"type\":\"rx\",\"source\":\"device\",\"hex\":\"0a\"}\n";
    QVector<TimelineEvent> loaded = {makeEvent(9, TimelineKind::Rx)};
    int badLine = 0;

    QVERIFY(!TimelineFile::fromJsonLines(good + "\nnot json\n", &loaded, &badLine));
    QCOMPARE(badLine, 3);
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.at(0).timestampMs, qint64(9));

    QVERIFY(!TimelineFile::fromJsonLines("{\"time_ms\":1,\"type\":\"rx\",\"source\":\"device\",\"hex\":\"0g\"}", &loaded, &badLine));
    QVERIFY(!TimelineFile::fromJsonLines("{\"time_ms\":1,\"type\":\"bogus\",\"source\":\"device\"}", &loaded, &badLine));
    QVERIFY(TimelineFile::fromJsonLines(good, &loaded, &badLine));
    QCOMPARE(loaded.size(), 1);
}

QTEST_MAIN(TimelineTest)
#include "tst_timeline.moc"
