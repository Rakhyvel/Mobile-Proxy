#include "queue.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct queueNode {
    Header header;
    char* data; // Allocate a new buffer on the heap for this!
    bool awaiting_ack;
    struct queueNode* next;
} QueueNode;

static QueueNode* queue = NULL;
static int num_msgs = 0;

// Pushes a message to be sent onto the back of the queue
void push_msg(MessageType type, int session_id, char* data, int num_bytes) {
    // create node
    QueueNode* node = calloc(sizeof(QueueNode), 1);
    node->header.type = type;
    node->header.length = num_bytes;
    node->header.session_id = session_id;
    node->header.msg_num = ++num_msgs;
    node->data = data;
    node->awaiting_ack = false;
    node->next = NULL;

    // Place at end of queue
    if (queue == NULL) {
        queue = node;
    } else {
        QueueNode* curr = queue;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = node;
    }
}

// Pops the front of the queue. NO CHECKS IF THIS IS GOOD OR NOT! MAYBE BUG!!
void pop_front() {
    // pop front of queue
    QueueNode* node = queue;
    queue = node->next;
    free(node->data);
}

// Sends the message at the front of the queue if it has not been sent already
void send_front(int sock) {
    // if the front message is not awaiting ack, send, mark awaiting_ack
    if (!queue->awaiting_ack) {
        // Send header
        send_raw(sock, (char*)(&queue->header), sizeof(queue->header));

        // Send data
        if (header.type == DATA) {
            send_raw(sock, queue->data, queue->header.length);
        }
        queue->awaiting_ack = true;
    }
}