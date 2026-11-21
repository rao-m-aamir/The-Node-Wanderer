#pragma once

#include "Node.h"

// Owns every node it creates. Each node links forward (next) and backward (previous).
// head->previous == nullptr and tail->next == nullptr.
class DoublyLinkedList {
public:
    DoublyLinkedList() = default;
    ~DoublyLinkedList(); // calls clear()

    DoublyLinkedList(const DoublyLinkedList&) = delete;
    DoublyLinkedList& operator=(const DoublyLinkedList&) = delete;

    Node* append(int id, const char* label, Vector2 position, float width);

    Node* getHead() const;
    Node* getTail() const;

    void clear();

private:
    Node* head = nullptr;
    Node* tail = nullptr;
};
