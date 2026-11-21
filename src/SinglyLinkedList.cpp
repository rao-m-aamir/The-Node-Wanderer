#include "SinglyLinkedList.h"

SinglyLinkedList::~SinglyLinkedList() {
    clear(); // safe to call again: clear() leaves head == nullptr
}

Node* SinglyLinkedList::append(int id, const char* label, Vector2 position, float width) {
    Node* newNode = new Node{id, label, position, width, nullptr, nullptr};
    g_liveNodeCount++;

    // Empty list: the new node becomes the head.
    if (head == nullptr) {
        head = newNode;
        return newNode;
    }

    // Walk forward until we find the tail (the node whose next is nullptr).
    Node* current = head;
    while (current->next != nullptr) {
        current = current->next;
    }

    current->next = newNode; // the old tail now links forward to the new node
    return newNode;
}

Node* SinglyLinkedList::getHead() const {
    return head;
}

void SinglyLinkedList::clear() {
    Node* current = head;
    while (current != nullptr) {
        Node* nextNode = current->next; // save the link BEFORE deleting the node
        delete current;
        g_liveNodeCount--;
        current = nextNode;
    }
    head = nullptr;
}
