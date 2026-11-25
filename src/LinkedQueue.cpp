#include "LinkedQueue.h"

LinkedQueue::~LinkedQueue() {
    clear(); // safe to call again: clear() leaves front == rear == nullptr
}

Node* LinkedQueue::enqueue(int id, const char* label, Vector2 position, float width) {
    Node* newNode = new Node{id, label, position, width, nullptr, nullptr};
    g_liveNodeCount++;

    // Empty queue: the new node is both the front and the rear.
    if (rear == nullptr) {
        front = newNode;
        rear = newNode;
        return newNode;
    }

    rear->next = newNode; // the old rear links to the new node...
    rear = newNode;       // ...which is the new rear. No walk through the list is needed.
    return newNode;
}

bool LinkedQueue::dequeue() {
    if (front == nullptr) {
        return false; // an empty queue has nothing to take: check before you take
    }
    Node* oldFront = front;
    front = front->next; // save the link first: the next node is the new front
    if (front == nullptr) {
        rear = nullptr; // that was the last node: the rear must not point at deleted memory
    }
    delete oldFront;
    g_liveNodeCount--;
    return true;
}

Node* LinkedQueue::getFront() const {
    return front;
}

Node* LinkedQueue::getRear() const {
    return rear;
}

bool LinkedQueue::isEmpty() const {
    return front == nullptr;
}

void LinkedQueue::clear() {
    while (dequeue()) {
        // dequeue() deletes one node at a time, from the front, until the queue is empty
    }
}
