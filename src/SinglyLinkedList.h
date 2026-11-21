#pragma once

#include "Node.h"

// Owns every node it creates. Each node links forward with `next` only.
// The last node (tail) has next == nullptr.
class SinglyLinkedList {
public:
    SinglyLinkedList() = default;
    ~SinglyLinkedList(); // calls clear()

    // An owning list must not be copied: two copies would delete the same nodes.
    SinglyLinkedList(const SinglyLinkedList&) = delete;
    SinglyLinkedList& operator=(const SinglyLinkedList&) = delete;

    // Allocates a new node at the end of the list and returns it.
    Node* append(int id, const char* label, Vector2 position, float width);

    Node* getHead() const;

    // Deletes every node exactly once and leaves the list empty.
    void clear();

private:
    Node* head = nullptr;
};
