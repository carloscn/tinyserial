// SPDX-License-Identifier: MPL-2.0

#include "timeline.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {

int kindIndex(TimelineKind kind)
{
    return static_cast<int>(kind);
}

QString escapeControlCharacters(const QString &text)
{
    QString escaped;
    escaped.reserve(text.size());
    for (const QChar ch : text) {
        const ushort code = ch.unicode();
        if (code == '\t')
            escaped += QStringLiteral("\\t");
        else if (code == '\r')
            escaped += QStringLiteral("\\r");
        else if (code == '\n')
            escaped += QStringLiteral("\\n");
        else if (code < 0x20 || code == 0x7f)
            escaped += QStringLiteral("\\x%1").arg(static_cast<uint>(code), 2, 16, QLatin1Char('0'));
        else
            escaped += ch;
    }
    return escaped;
}

QString asciiText(const QByteArray &data)
{
    QByteArray line = data;
    while (line.endsWith('\n') || line.endsWith('\r'))
        line.chop(1);

    const QString utf8 = QString::fromUtf8(line);
    if (!utf8.contains(QChar::ReplacementCharacter))
        return escapeControlCharacters(utf8);

    // Not valid UTF-8: keep printable ASCII and show every other byte as \xNN.
    QString text;
    text.reserve(line.size());
    for (const char byte : line) {
        const uchar value = static_cast<uchar>(byte);
        if (value >= 0x80)
            text += QStringLiteral("\\x%1").arg(static_cast<uint>(value), 2, 16, QLatin1Char('0'));
        else
            text += escapeControlCharacters(QString(QLatin1Char(byte)));
    }
    return text;
}

QString hexText(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

QString formatDelta(qint64 deltaMs)
{
    if (deltaMs < 1000)
        return QStringLiteral("+%1 ms").arg(deltaMs);
    return QStringLiteral("+%1 s").arg(static_cast<double>(deltaMs) / 1000.0, 0, 'f', 3);
}

QString csvField(const QString &value)
{
    if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"'))
        && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r'))) {
        return value;
    }
    QString quoted = value;
    quoted.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

bool kindFromCode(const QString &code, TimelineKind *kind)
{
    for (TimelineKind candidate : {TimelineKind::Rx, TimelineKind::Tx, TimelineKind::System}) {
        if (timelineKindCode(candidate) == code) {
            *kind = candidate;
            return true;
        }
    }
    return false;
}

bool sourceFromCode(const QString &code, TimelineSource *source)
{
    for (TimelineSource candidate : {TimelineSource::Device, TimelineSource::Manual,
                                     TimelineSource::Repeat, TimelineSource::App}) {
        if (timelineSourceCode(candidate) == code) {
            *source = candidate;
            return true;
        }
    }
    return false;
}

bool isHexString(const QString &text)
{
    if (text.size() % 2 != 0)
        return false;
    for (const QChar ch : text) {
        if (!ch.isDigit() && !(ch.toLower() >= QLatin1Char('a') && ch.toLower() <= QLatin1Char('f')))
            return false;
    }
    return true;
}

} // namespace

QString timelineKindCode(TimelineKind kind)
{
    switch (kind) {
    case TimelineKind::Rx: return QStringLiteral("rx");
    case TimelineKind::Tx: return QStringLiteral("tx");
    case TimelineKind::System: return QStringLiteral("system");
    }
    return QStringLiteral("system");
}

QString timelineSourceCode(TimelineSource source)
{
    switch (source) {
    case TimelineSource::Device: return QStringLiteral("device");
    case TimelineSource::Manual: return QStringLiteral("manual");
    case TimelineSource::Repeat: return QStringLiteral("repeat");
    case TimelineSource::App: return QStringLiteral("app");
    }
    return QStringLiteral("app");
}

QString timelineContentText(const TimelineEvent &event, bool hex)
{
    if (event.kind == TimelineKind::System)
        return event.message;
    return hex ? hexText(event.data) : asciiText(event.data);
}

TimelineLineAssembler::TimelineLineAssembler(int maxLineBytes)
    : maxLineBytes(qMax(1, maxLineBytes))
{
}

QVector<TimelineEvent> TimelineLineAssembler::append(const QByteArray &chunk, qint64 timestampMs)
{
    QVector<TimelineEvent> lines;
    for (const char byte : chunk) {
        if (pending.isEmpty())
            pendingStartMs = timestampMs;
        pending.append(byte);
        if (byte == '\n' || pending.size() >= maxLineBytes)
            lines.append(takePending());
    }
    return lines;
}

QVector<TimelineEvent> TimelineLineAssembler::flush()
{
    QVector<TimelineEvent> lines;
    if (!pending.isEmpty())
        lines.append(takePending());
    return lines;
}

bool TimelineLineAssembler::hasPending() const
{
    return !pending.isEmpty();
}

void TimelineLineAssembler::clear()
{
    pending.clear();
}

TimelineEvent TimelineLineAssembler::takePending()
{
    TimelineEvent event;
    event.timestampMs = pendingStartMs;
    event.kind = TimelineKind::Rx;
    event.source = TimelineSource::Device;
    event.data = pending;
    pending.clear();
    return event;
}

QByteArray TimelineFile::toCsv(const QVector<TimelineEvent> &events)
{
    // The byte order mark makes Excel open the file as UTF-8.
    QString csv = QString(QChar(0xFEFF)) + QStringLiteral("time,delta_ms,type,source,bytes,text,hex\r\n");
    qint64 previousMs = -1;
    for (const TimelineEvent &event : events) {
        const bool hasData = event.kind != TimelineKind::System;
        QStringList fields;
        fields << QDateTime::fromMSecsSinceEpoch(event.timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
               << (previousMs < 0 ? QString() : QString::number(event.timestampMs - previousMs))
               << timelineKindCode(event.kind)
               << timelineSourceCode(event.source)
               << (hasData ? QString::number(event.data.size()) : QString())
               << csvField(timelineContentText(event, false))
               << (hasData ? hexText(event.data) : QString());
        csv += fields.join(QLatin1Char(',')) + QStringLiteral("\r\n");
        previousMs = event.timestampMs;
    }
    return csv.toUtf8();
}

QByteArray TimelineFile::toJsonLines(const QVector<TimelineEvent> &events)
{
    QByteArray output;
    for (const TimelineEvent &event : events) {
        QJsonObject object;
        object.insert(QStringLiteral("time_ms"), static_cast<double>(event.timestampMs));
        object.insert(QStringLiteral("time"),
                      QDateTime::fromMSecsSinceEpoch(event.timestampMs).toString(Qt::ISODateWithMs));
        object.insert(QStringLiteral("type"), timelineKindCode(event.kind));
        object.insert(QStringLiteral("source"), timelineSourceCode(event.source));
        if (event.kind == TimelineKind::System)
            object.insert(QStringLiteral("message"), event.message);
        else
            object.insert(QStringLiteral("hex"), QString::fromLatin1(event.data.toHex()));
        output += QJsonDocument(object).toJson(QJsonDocument::Compact);
        output += '\n';
    }
    return output;
}

bool TimelineFile::fromJsonLines(const QByteArray &content, QVector<TimelineEvent> *events, int *badLine)
{
    QVector<TimelineEvent> parsed;
    const QList<QByteArray> lines = content.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty())
            continue;

        const QJsonDocument document = QJsonDocument::fromJson(line);
        const QJsonObject object = document.object();
        TimelineEvent event;
        const QString hex = object.value(QStringLiteral("hex")).toString();
        const bool valid = document.isObject()
            && object.value(QStringLiteral("time_ms")).isDouble()
            && kindFromCode(object.value(QStringLiteral("type")).toString(), &event.kind)
            && sourceFromCode(object.value(QStringLiteral("source")).toString(), &event.source)
            && isHexString(hex);
        if (!valid) {
            if (badLine)
                *badLine = i + 1;
            return false;
        }

        event.timestampMs = static_cast<qint64>(object.value(QStringLiteral("time_ms")).toDouble());
        event.data = QByteArray::fromHex(hex.toLatin1());
        event.message = object.value(QStringLiteral("message")).toString();
        parsed.append(event);
    }

    if (events)
        *events = parsed;
    return true;
}

TimelineModel::TimelineModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int TimelineModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(events.size());
}

int TimelineModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant TimelineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return QVariant();

    const TimelineEvent &event = events.at(static_cast<size_t>(index.row()));
    switch (role) {
    case KindRole:
        return static_cast<int>(event.kind);
    case SearchTextRole:
        return timelineContentText(event, false);
    case Qt::DisplayRole:
        switch (index.column()) {
        case TimeColumn:
            return QDateTime::fromMSecsSinceEpoch(event.timestampMs).toString(QStringLiteral("HH:mm:ss.zzz"));
        case DeltaColumn:
            if (index.row() == 0)
                return QString::fromUtf8("—");
            return formatDelta(event.timestampMs - events.at(static_cast<size_t>(index.row() - 1)).timestampMs);
        case TypeColumn:
            return event.kind == TimelineKind::Rx ? QStringLiteral("RX")
                : event.kind == TimelineKind::Tx ? QStringLiteral("TX") : QStringLiteral("SYS");
        case SourceColumn:
            switch (event.source) {
            case TimelineSource::Device: return label("source_device", "Device");
            case TimelineSource::Manual: return label("source_manual", "Manual");
            case TimelineSource::Repeat: return label("source_repeat", "Repeat");
            case TimelineSource::App: return QString::fromUtf8("—");
            }
            return QVariant();
        case BytesColumn:
            return event.kind == TimelineKind::System ? QVariant() : QVariant(event.data.size());
        case ContentColumn:
            return timelineContentText(event, hexContent);
        }
        return QVariant();
    case Qt::ToolTipRole:
        if (index.column() == TimeColumn)
            return QDateTime::fromMSecsSinceEpoch(event.timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
        if (index.column() == ContentColumn)
            return timelineContentText(event, hexContent);
        return QVariant();
    case Qt::TextAlignmentRole:
        if (index.column() == DeltaColumn || index.column() == BytesColumn)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::FontRole:
        if (index.column() == ContentColumn || index.column() == TimeColumn)
            return contentFont;
        return QVariant();
    case Qt::ForegroundRole:
        if (index.column() != TypeColumn)
            return QVariant();
        if (event.kind == TimelineKind::Rx)
            return QColor(QRgb(darkMode ? 0xff713d : 0xd93900));
        if (event.kind == TimelineKind::Tx)
            return QColor(QRgb(darkMode ? 0x4fc49a : 0x147d5b));
        return QColor(QRgb(darkMode ? 0x9babb4 : 0x61757f));
    case Qt::BackgroundRole:
        if (inResponseOf(index.row()))
            return QColor(QRgb(darkMode ? 0x3a241c : 0xfff1eb));
        return QVariant();
    }
    return QVariant();
}

QVariant TimelineModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case TimeColumn: return label("col_time", "Time");
    case DeltaColumn: return label("col_delta", "Since previous");
    case TypeColumn: return label("col_type", "Type");
    case SourceColumn: return label("col_source", "Source");
    case BytesColumn: return label("col_bytes", "Bytes");
    case ContentColumn: return label("col_content", "Content");
    }
    return QVariant();
}

void TimelineModel::setTextProvider(TextProvider provider)
{
    textProvider = std::move(provider);
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
    refreshAll();
}

void TimelineModel::setHexContent(bool hex)
{
    if (hexContent == hex)
        return;
    hexContent = hex;
    refreshAll();
}

void TimelineModel::setDarkMode(bool dark)
{
    if (darkMode == dark)
        return;
    darkMode = dark;
    refreshAll();
}

void TimelineModel::setContentFont(const QFont &font)
{
    contentFont = font;
    refreshAll();
}

void TimelineModel::setMaxEvents(int newMaxEvents)
{
    maxEvents = qMax(1, newMaxEvents);
    trimToMax();
}

void TimelineModel::append(const QVector<TimelineEvent> &newEvents)
{
    if (newEvents.isEmpty())
        return;

    const int first = rowCount();
    beginInsertRows(QModelIndex(), first, first + newEvents.size() - 1);
    for (const TimelineEvent &event : newEvents) {
        events.push_back(event);
        ++kindCounts[kindIndex(event.kind)];
    }
    endInsertRows();
    if (responseAnchor >= 0 && responseEnd == first)
        updateResponseEnd(first);
    trimToMax();
}

void TimelineModel::setEvents(const QVector<TimelineEvent> &newEvents)
{
    beginResetModel();
    events.assign(newEvents.cbegin(), newEvents.cend());
    responseAnchor = -1;
    recount();
    endResetModel();
    trimToMax();
}

void TimelineModel::clear()
{
    setEvents(QVector<TimelineEvent>());
}

const TimelineEvent &TimelineModel::eventAt(int row) const
{
    return events.at(static_cast<size_t>(row));
}

QVector<TimelineEvent> TimelineModel::allEvents() const
{
    QVector<TimelineEvent> copy;
    copy.reserve(static_cast<int>(events.size()));
    for (const TimelineEvent &event : events)
        copy.append(event);
    return copy;
}

int TimelineModel::count(TimelineKind kind) const
{
    return kindCounts[kindIndex(kind)];
}

void TimelineModel::setResponseAnchor(int txRow)
{
    const int anchor = (txRow >= 0 && txRow < rowCount() && eventAt(txRow).kind == TimelineKind::Tx) ? txRow : -1;
    if (anchor == responseAnchor)
        return;
    responseAnchor = anchor;
    updateResponseEnd(anchor + 1);
    refreshAll();
}

TimelineModel::Response TimelineModel::responseTo(int txRow) const
{
    Response response;
    if (txRow < 0 || txRow >= rowCount())
        return response;

    const qint64 sentMs = eventAt(txRow).timestampMs;
    for (int row = txRow + 1; row < rowCount(); ++row) {
        const TimelineEvent &event = eventAt(row);
        if (event.kind == TimelineKind::Tx)
            break;
        if (event.kind != TimelineKind::Rx)
            continue;
        if (response.firstLatencyMs < 0)
            response.firstLatencyMs = event.timestampMs - sentMs;
        ++response.rxEvents;
        response.rxBytes += event.data.size();
    }
    return response;
}

void TimelineModel::trimToMax()
{
    const int size = rowCount();
    if (size <= maxEvents)
        return;

    // Drop a little extra so a steady stream does not trim on every append.
    const int removeCount = qMin(size, size - maxEvents + maxEvents / 20);
    beginRemoveRows(QModelIndex(), 0, removeCount - 1);
    for (int i = 0; i < removeCount; ++i) {
        --kindCounts[kindIndex(events.front().kind)];
        events.pop_front();
    }
    endRemoveRows();

    responseAnchor = responseAnchor >= removeCount ? responseAnchor - removeCount : -1;
    responseEnd = qMax(0, responseEnd - removeCount);
    if (rowCount() > 0)
        emit dataChanged(index(0, DeltaColumn), index(0, DeltaColumn));
}

void TimelineModel::recount()
{
    kindCounts[0] = kindCounts[1] = kindCounts[2] = 0;
    for (const TimelineEvent &event : events)
        ++kindCounts[kindIndex(event.kind)];
}

bool TimelineModel::inResponseOf(int row) const
{
    return responseAnchor >= 0 && row > responseAnchor && row < responseEnd
        && eventAt(row).kind == TimelineKind::Rx;
}

void TimelineModel::updateResponseEnd(int from)
{
    // responseEnd is the first TX after the anchor, or the row count when none follows yet.
    responseEnd = from;
    if (responseAnchor < 0)
        return;
    while (responseEnd < rowCount() && eventAt(responseEnd).kind != TimelineKind::Tx)
        ++responseEnd;
}

QString TimelineModel::label(const char *key, const char *fallback) const
{
    return textProvider ? textProvider(key) : QString::fromLatin1(fallback);
}

void TimelineModel::refreshAll()
{
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
}

TimelineFilterModel::TimelineFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void TimelineFilterModel::setKindVisible(TimelineKind kind, bool visible)
{
    if (kindVisible[kindIndex(kind)] == visible)
        return;
    kindVisible[kindIndex(kind)] = visible;
    invalidateFilter();
}

void TimelineFilterModel::setSearchText(const QString &text)
{
    if (searchText == text)
        return;
    searchText = text;
    invalidateFilter();
}

bool TimelineFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const int kind = index.data(TimelineModel::KindRole).toInt();
    if (kind < 0 || kind > 2 || !kindVisible[kind])
        return false;
    if (searchText.isEmpty())
        return true;
    return index.data(TimelineModel::SearchTextRole).toString().contains(searchText, Qt::CaseInsensitive);
}
