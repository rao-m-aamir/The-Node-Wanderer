#pragma once

#include "Node.h"

// A queue built from a singly linked list with a pointer to each end. enqueue adds a node at
// the rear, and dequeue takes the node at the front off. The first node enqueued is the first
// one dequeued (first in, first out). Keeping `rear` means enqueue never has to walk the list.
// Owns every node it creates.
class LinkedQueue {
public:
    LinkedQueue() = default;
    ~LinkedQueue(); // calls clear()

    // An owning list must not be copied: two copies would delete the same nodes.
    LinkedQueue(const LinkedQueue&) = delete;
    LinkedQueue& operator=(const LinkedQueue&) = delete;

    // Allocates a new node at the rear and returns it.
    Node* enqueue(int id, const char* label, Vector2 position, float width);

    // Deletes the front node. Returns false (and does nothing) if the queue is empty.
    bool dequeue();

    Node* getFront() const;
    Node* getRear() const;
    bool isEmpty() const;

    // Deletes every node exactly once and leaves the queue empty.
    void clear();

private:
    Node* front = nullptr;
    Node* rear = nullptr;
};
