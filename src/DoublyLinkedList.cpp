#include "DoublyLinkedList.h"

DoublyLinkedList::~DoublyLinkedList() {
    clear();
}

Node* DoublyLinkedList::append(int id, const char* label, Vector2 position, float width) {
    Node* newNode = new Node{id, label, position, width, nullptr, nullptr};
    g_liveNodeCount++;

    // Empty list: the new node is both head and tail.
    if (head == nullptr) {
        head = newNode;
        tail = newNode;
        return newNode;
    }

    tail->next = newNode;     // old tail links forward to the new node
    newNode->previous = tail; // new node links back to the old tail
    tail = newNode;           // the new node is now the tail
    return newNode;
}

Node* DoublyLinkedList::getHead() const {
    return head;
}

Node* DoublyLinkedList::getTail() const {
    return tail;
}

void DoublyLinkedList::clear() {
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
