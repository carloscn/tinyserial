#ifndef DISPLAYBUFFER_H
#define DISPLAYBUFFER_H

#include <QString>

class DisplayBuffer
{
public:
    explicit DisplayBuffer(int capacity);

    void append(const QString &text);
    QString take();
    void clear();
    bool isEmpty() const;
    int size() const;

private:
    int capacity;
    QString contents;
};

#endif // DISPLAYBUFFER_H
