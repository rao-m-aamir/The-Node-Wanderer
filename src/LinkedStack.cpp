#include "LinkedStack.h"

LinkedStack::~LinkedStack() {
    clear(); // safe to call again: clear() leaves top == nullptr
}

Node* LinkedStack::push(int id, const char* label, Vector2 position, float width) {
    Node* newNode = new Node{id, label, position, width, nullptr, nullptr};
    g_liveNodeCount++;

    newNode->next = top; // the new node sits on the old top (nullptr if the stack was empty)
    top = newNode;       // and becomes the top itself
    return newNode;
}

bool LinkedStack::pop() {
    if (top == nullptr) {
        return false; // an empty stack has nothing to pop: check before you take
    }
    Node* oldTop = top;
    top = top->next; // save the link first: the node under the top becomes the top
    delete oldTop;
    g_liveNodeCount--;
    return true;
}

Node* LinkedStack::getTop() const {
    return top;
}

bool LinkedStack::isEmpty() const {
    return top == nullptr;
}

void LinkedStack::clear() {
    while (pop()) {
        // pop() deletes one node at a time, from the top down, until top == nullptr
    }
}
