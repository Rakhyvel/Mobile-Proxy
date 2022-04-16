/*
The message queue stores messages that have been sent or are waiting to be
sent. Messages are popped off the queue when they are acknowledged by the other
proxy.
*/

#include "queue.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct queueNode {
    Header header;
    char* data; // will allocate a new buffer on the heap for this!
    bool awaiting_ack;
    struct queueNode* next;
} QueueNode;

static QueueNode* queue = NULL;

/*
Pushes a message to be sent onto the back of the queue

@param type Type of the message to send
@param session_id Session ID of the sender
@param data Pointer to data of the message. Will be copied
@param num_bytes The number of bytes in the message's data
*/
void push_msg(MessageType type, int session_id, char* data, int num_bytes) {
    // create node
    QueueNode* node = calloc(sizeof(QueueNode), 1);
    node->header.type = type;
    node->header.length = num_bytes;
    node->header.session_id = session_id;
    node->data = calloc(num_bytes, sizeof(char));
    memcpy(node->data, data, num_bytes);
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

/*
Pops the front of the queue. OK to pop an empty queue, though that shouldn't 
happen in normal usage.
*/
void pop_front() {
    // pop front of queue
    QueueNode* node = queue;
    if(queue == NULL){
      return;
    }
    queue = node->next;
    free(node->data);
}

/*
Sends the message at the front of the queue if it has not been sent already

@param sock Socket of the other proxy to send the message to
*/
void send_front(int sock) {
    // if the front message is not awaiting ack, send, mark awaiting_ack
    if (queue && !queue->awaiting_ack) {
        send_header(sock, queue->data, queue->header);
        queue->awaiting_ack = true;
    }
}

/*
Resets the first message of the queue so that it isnt awaiting an ack
*/
void reset_await_status() {
    if (queue != NULL) {
        queue->awaiting_ack = false;
    }
}

static void destroy_queue_node(QueueNode* node) {
    if (node != NULL) {
        destroy_queue_node(node->next);
        free(node->data);
    }
}

/*
Destroys the queue
*/
void reset_queue() {
    destroy_queue_node(queue);
    queue = NULL;
}
