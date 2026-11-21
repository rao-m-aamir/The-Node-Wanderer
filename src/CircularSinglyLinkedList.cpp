#include "CircularSinglyLinkedList.h"

CircularSinglyLinkedList::~CircularSinglyLinkedList() {
    clear();
}

Node* CircularSinglyLinkedList::append(int id, const char* label, Vector2 position, float width) {
    Node* newNode = new Node{id, label, position, width, nullptr, nullptr};
    g_liveNodeCount++;

    // Empty list: a single node is its own successor.
    if (head == nullptr) {
        head = newNode;
        tail = newNode;
        newNode->next = newNode; // one-node circle
        return newNode;
    }

    tail->next = newNode; // old tail links forward to the new node
    newNode->next = head; // new tail closes the circle back to the head
    tail = newNode;
    return newNode;
}

Node* CircularSinglyLinkedList::getHead() const {
    return head;
}

Node* CircularSinglyLinkedList::getTail() const {
    return tail;
}

void CircularSinglyLinkedList::clear() {
    if (head == nullptr) {
        return; // nothing to delete
    }

    // Break the cycle first. Without this, the loop below would never
    // reach nullptr and would delete the head a second time.
    tail->next = nullptr;

    Node* current = head;
    while (current != nullptr) {
        Node* nextNode = current->next; // save the link BEFORE deleting the node
        delete current;
        g_liveNodeCount--;
        current = nextNode;
    }
    head = nullptr;
    tail = nullptr;
}
