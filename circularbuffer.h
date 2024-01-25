#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#include "qvector.h"

class CircularBuffer {
private:
    QVector<QVector<float>> buffer;
    int size;
    int head;
    int tail;
public:
    CircularBuffer(int buffer_size) : size(buffer_size), head(0), tail(0) {
        buffer.resize(size);
    }

    void push(const QVector<float>& item) {
        buffer[head] = item;
        head = (head + 1) % size;
        if (head == tail) {
            tail = (tail + 1) % size;
        }
    }

    QVector<float> pop() {
        if (tail == head) {
            // Buffer is empty
            return QVector<float>();
        }
        QVector<float> item = buffer[tail];
        tail = (tail + 1) % size;
        return item;
    }

    bool isEmpty() const {
        return tail == head;
    }

    bool isFull() const {
        return (head + 1) % size == tail;
    }
};

#endif // CIRCULARBUFFER_H
