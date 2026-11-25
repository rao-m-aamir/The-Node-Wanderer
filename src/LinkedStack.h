#pragma once

#include "Node.h"

// A stack built from a singly linked list. Everything happens at one end, the top:
// push puts a new node on top, and pop takes the top node off. The last node pushed is the
// first one popped (last in, first out). The bottom node's next is nullptr.
// Owns every node it creates.
class LinkedStack {
public:
    LinkedStack() = default;
    ~LinkedStack(); // calls clear()

    // An owning list must not be copied: two copies would delete the same nodes.
    LinkedStack(const LinkedStack&) = delete;
    LinkedStack& operator=(const LinkedStack&) = delete;

    // Allocates a new node on top of the stack and returns it.
    Node* push(int id, const char* label, Vector2 position, float width);

    // Deletes the top node. Returns false (and does nothing) if the stack is empty.
    bool pop();

    Node* getTop() const;
    bool isEmpty() const;

    // Deletes every node exactly once and leaves the stack empty.
    void clear();

private:
    Node* top = nullptr;
};
