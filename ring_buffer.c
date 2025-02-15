// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "ring_buffer.h"
#include "packet.h"

void init_syncronization_primitives(so_ring_buffer_t *ring)
{
	pthread_mutex_init(&ring->mutex, NULL);
	pthread_cond_init(&ring->cond_full, NULL);
	pthread_cond_init(&ring->cond_empty, NULL);
}

int ring_buffer_init(so_ring_buffer_t *ring, size_t cap)
{
	/* TODO: implement ring_buffer_init */
	ring->data = malloc(cap);
	init_syncronization_primitives(ring);

	if (ring->data) {
		ring->read_pos = 0;
		ring->write_pos = 0;
		ring->len = 0;
		ring->cap = cap;
		ring->stop = 0;

		ring->seq_control.seq_to_timestamp = malloc(20001 * sizeof(unsigned long));
		ring->seq_control.id_next_nr = 0;
		ring->seq_control.seq_counter = 0;
		return 1;
	} else {
		return -1;
	}
}

ssize_t ring_buffer_enqueue(so_ring_buffer_t *ring, void *data, size_t size)
{
	while (ring->len + size > ring->cap)
		pthread_cond_wait(&ring->cond_full, &ring->mutex);

	size_t remaining = size;
	const char *src = (const char *)data;
	size_t end_space = ring->cap - ring->write_pos;

	size_t to_write = (remaining < end_space) ? remaining : end_space;

	memcpy(ring->data + ring->write_pos, src, to_write);

	remaining -= to_write;
	src += to_write;
	ring->write_pos = (ring->write_pos + to_write) % ring->cap;

	if (remaining > 0) {
		memcpy(ring->data, src, remaining);
		ring->write_pos = remaining;
	}

	struct so_packet_t *pkt = (struct so_packet_t *)data;

	ring->seq_control.seq_to_timestamp[ring->seq_control.seq_counter] = pkt->hdr.timestamp;
	ring->seq_control.seq_counter++;

	ring->len += size;

	pthread_cond_signal(&ring->cond_empty);

	return size;
}

ssize_t ring_buffer_dequeue(so_ring_buffer_t *ring, void *data, size_t size)
{
	while (ring->len == 0) {
		if (ring->stop) {
			pthread_mutex_unlock(&ring->mutex);
			return 0;
		}
		pthread_cond_wait(&ring->cond_empty, &ring->mutex);
	}

	size_t bytes_to_copy = size;
	size_t current_pos = ring->read_pos;
	char *dest = (char *)data;

	while (bytes_to_copy > 0) {
		size_t chunk_size = ring->cap - current_pos;

		if (chunk_size > bytes_to_copy)
			chunk_size = bytes_to_copy;

		memcpy(dest, ring->data + current_pos, chunk_size);
		dest += chunk_size;
		current_pos = (current_pos + chunk_size) % ring->cap;
		bytes_to_copy -= chunk_size;
	}

	ring->read_pos = current_pos;
	ring->len -= size;

	pthread_cond_signal(&ring->cond_full);

	return size;
}

void destroy_syncronization_primitives(so_ring_buffer_t *ring)
{
	pthread_mutex_destroy(&ring->mutex);
	pthread_cond_destroy(&ring->cond_full);
	pthread_cond_destroy(&ring->cond_empty);
}

void ring_buffer_destroy(so_ring_buffer_t *ring)
{
	/* TODO: Implement ring_buffer_destroy */
	free(ring->data);
	free(ring->seq_control.seq_to_timestamp);
	destroy_syncronization_primitives(ring);
}

void ring_buffer_stop_helper(so_ring_buffer_t *ring)
{
	ring->stop = 1;
	pthread_cond_broadcast(&ring->cond_empty);
	pthread_cond_broadcast(&ring->cond_full);
}

void ring_buffer_stop(so_ring_buffer_t *ring)
{
	/* TODO: Implement ring_buffer_stop */
	pthread_mutex_lock(&ring->mutex);
	ring_buffer_stop_helper(ring);
	pthread_mutex_unlock(&ring->mutex);
}
