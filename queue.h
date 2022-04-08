#ifndef QUEUE_H
#define QUEUE_H

#include "message.h"

void push_msg(MessageType type, int session_id, char* data, int num_bytes);
void pop_front();
void send_front();

#endif