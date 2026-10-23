// SPDX-License-Identifier: MPL-2.0

#ifndef TIMELINE_H
#define TIMELINE_H

#include <QAbstractTableModel>
#include <QByteArray>
#include <QColor>
#include <QFont>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVector>

#include <deque>
#include <functional>

enum class TimelineKind {
    Rx,
    Tx,
    System
};

enum class TimelineSource {
    Device,
    Manual,
    Repeat,
    App
};

struct TimelineEvent
{
    qint64 timestampMs = 0;   // milliseconds since the Unix epoch
    TimelineKind kind = TimelineKind::System;
    TimelineSource source = TimelineSource::App;
    QByteArray data;          // raw bytes of an RX or TX event
    QString message;          // text of a system event
};

QString timelineKindCode(TimelineKind kind);
QString timelineSourceCode(TimelineSource source);

// One-line text for an event: system messages as-is, data as HEX or as ASCII
// with the line ending dropped and other control bytes escaped.
QString timelineContentText(const TimelineEvent &event, bool hex);

// Turns the arbitrary chunks QSerialPort delivers into one event per line.
// A line ends at '\n', when it reaches maxLineBytes, or when flush() is
// called after the device has been quiet for a moment.
class TimelineLineAssembler
{
public:
    explicit TimelineLineAssembler(int maxLineBytes = 4096);

    QVector<TimelineEvent> append(const QByteArray &chunk, qint64 timestampMs);
    QVector<TimelineEvent> flush();
    bool hasPending() const;
    void clear();

private:
    TimelineEvent takePending();

    QByteArray pending;
    qint64 pendingStartMs = 0;
    int maxLineBytes;
};

namespace TimelineFile {
QByteArray toCsv(const QVector<TimelineEvent> &events);
QByteArray toJsonLines(const QVector<TimelineEvent> &events);
// Returns false and the 1-based number of the first bad line on malformed input.
bool fromJsonLines(const QByteArray &content, QVector<TimelineEvent> *events, int *badLine = nullptr);
}

class TimelineModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        TimeColumn,
        DeltaColumn,
        TypeColumn,
        SourceColumn,
        BytesColumn,
        ContentColumn,
        ColumnCount
    };

    enum Role {
        KindRole = Qt::UserRole + 1,
        SearchTextRole
    };

    struct Response
    {
        int rxEvents = 0;
        qint64 rxBytes = 0;
        qint64 firstLatencyMs = -1;
    };

    using TextProvider = std::function<QString(const char *key)>;

    explicit TimelineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setTextProvider(TextProvider provider);
    void setHexContent(bool hex);
    void setDarkMode(bool dark);
    void setContentFont(const QFont &font);
    void setMaxEvents(int maxEvents);

    void append(const QVector<TimelineEvent> &newEvents);
    void setEvents(const QVector<TimelineEvent> &newEvents);
    void clear();

    const TimelineEvent &eventAt(int row) const;
    QVector<TimelineEvent> allEvents() const;
    int count(TimelineKind kind) const;

    // Highlights the RX events that follow a TX row until the next TX.
    void setResponseAnchor(int txRow);
    Response responseTo(int txRow) const;

private:
    void trimToMax();
    void recount();
    bool inResponseOf(int row) const;
    void updateResponseEnd(int from);
    QString label(const char *key, const char *fallback) const;
    void refreshAll();

    std::deque<TimelineEvent> events;
    TextProvider textProvider;
    QFont contentFont;
    int maxEvents = 100000;
    int responseAnchor = -1;
    int responseEnd = 0;
    int kindCounts[3] = {0, 0, 0};
    bool hexContent = false;
    bool darkMode = false;
};

class TimelineFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit TimelineFilterModel(QObject *parent = nullptr);

    void setKindVisible(TimelineKind kind, bool visible);
    void setSearchText(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool kindVisible[3] = {true, true, true};
    QString searchText;
};

#endif // TIMELINE_H
