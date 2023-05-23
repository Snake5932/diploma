#ifndef QUEUE
#define QUEUE

#include "stdint.h"


struct queue;

struct queue_data {
	void** data;
	int count;
};

struct queue* init_queue(int n);

void* peek(struct queue* q);

struct queue_data get_all(struct queue* q);

void* dequeue(struct queue* q);

void enqueue(struct queue* q, void* v);

int is_empty(struct queue* q);

void flush_all(struct queue* q);

void free_queue(struct queue* q);

int get_queue_len(struct queue* q);

#endif
