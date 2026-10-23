#include "displaybuffer.h"

DisplayBuffer::DisplayBuffer(int capacity)
    : capacity(qMax(0, capacity))
{
}

void DisplayBuffer::append(const QString &text)
{
    if (capacity == 0 || text.isEmpty())
        return;
    if (text.size() >= capacity) {
        contents = text.right(capacity);
        return;
    }

    const int overflow = contents.size() + text.size() - capacity;
    if (overflow > 0)
        contents.remove(0, overflow);
    contents.append(text);
}

QString DisplayBuffer::take()
{
    QString result;
    result.swap(contents);
    return result;
}

void DisplayBuffer::clear()
{
    contents.clear();
}

bool DisplayBuffer::isEmpty() const
{
    return contents.isEmpty();
}

int DisplayBuffer::size() const
{
    return contents.size();
}
