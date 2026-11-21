#pragma once

#include "Node.h"

// Owns every node it creates. Each node links forward with `next`,
// and the tail's next points back to the head, so no next is ever nullptr.
// append() keeps the circle closed after every insertion.
class CircularSinglyLinkedList {
public:
    CircularSinglyLinkedList() = default;
    ~CircularSinglyLinkedList(); // calls clear()

    CircularSinglyLinkedList(const CircularSinglyLinkedList&) = delete;
    CircularSinglyLinkedList& operator=(const CircularSinglyLinkedList&) = delete;

    Node* append(int id, const char* label, Vector2 position, float width);

    Node* getHead() const;
    Node* getTail() const;

    void clear();

private:
    Node* head = nullptr;
    Node* tail = nullptr;
};
