#include "queue.h"
#include "stdlib.h"
#include "stdint.h"

struct queue {
	void** data;
	int cap;
	int count;
	int head;
	int tail;
};

struct queue* init_queue(int n) {
	struct queue* q = (struct queue*)malloc(sizeof(struct queue));
	q->cap = n;
	q->count = 0;
	q->head = 0;
	q->tail = 0;
	q->data = (void**)malloc(n * sizeof(void*));
	for (int i = 0; i < q->cap; i++) {
		void* ptr = malloc(sizeof(int));
		q->data[i] = ptr;
	}
	return q;
}

void* peek(struct queue* q) {
	void* ptr = q->data[q->head];
	return ptr;
}

void* dequeue(struct queue* q) {
	void* ptr = q->data[q->head];
	q->data[q->head] = malloc(sizeof(int));
	q->head = q->head + 1;
	if (q->head == q->cap) {
		q->head = 0;
	}
	q->count = q->count - 1;
	return ptr;
}

void enqueue(struct queue* q, void* v) {
	if (q->count == q->cap) {
		void* ptr = dequeue(q);
		free(ptr);
	}
	free(q->data[q->tail]);
	q->data[q->tail] = v;
	q->tail = q->tail + 1;
	if (q->tail == q->cap) {
		q->tail = 0;
	}
	q->count = q->count + 1;
}

void flush_all(struct queue* q) {
	q->count = 0;
	q->head = 0;
	q->tail = 0;
}

int is_empty(struct queue* q) {
	return q->count == 0;
}

void free_queue(struct queue* q) {
	for (int i = 0; i < q->cap; i++) {
		free(q->data[i]);
	}
	free(q->data);
	free(q);
}

int get_queue_len(struct queue* q) {
	return q->count;
}

struct queue_data get_all(struct queue* q) {
	return (struct queue_data){q->data, q->count};
}
