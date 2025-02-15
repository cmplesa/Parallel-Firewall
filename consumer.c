// SPDX-License-Identifier: BSD-3-Clause

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "consumer.h"
#include "ring_buffer.h"
#include "packet.h"
#include "utils.h"

void *consumer_thread(void *args)
{
	so_consumer_ctx_t *ctx = (so_consumer_ctx_t *)args;
	char *buffer = malloc(PKT_SZ);
	char *out_buf = malloc(PKT_SZ);

	memset(buffer, 0, PKT_SZ);

	while (1) {
		struct so_packet_t *pkt;

		pthread_mutex_lock(&ctx->producer_rb->mutex);
		size_t ret = ring_buffer_dequeue(ctx->producer_rb, buffer, PKT_SZ);

		pthread_mutex_unlock(&ctx->producer_rb->mutex);

		if (ret <= 0)
			break;

		pkt = (struct so_packet_t *)buffer;
		unsigned long timestamp = pkt->hdr.timestamp;
		// inline the logic of timestamp_to_seq
		size_t seq = -1;

		for (size_t i = 0; i < ctx->producer_rb->seq_control.seq_counter; i++) {
			if (ctx->producer_rb->seq_control.seq_to_timestamp[i] == timestamp) {
				seq = i;
				break;
			}
		}

		int action = process_packet(pkt);
		const char *action_str = RES_TO_STR(action);
		size_t action_len = strlen(action_str);

		memcpy(out_buf, action_str, action_len);

		unsigned long hash = packet_hash(pkt);

		out_buf[action_len] = ' ';

		snprintf(out_buf + action_len + 1, 17, "%016lx", hash);
		size_t hash_len = 16;

		out_buf[action_len + 1 + hash_len] = ' ';

		snprintf(out_buf + action_len + 1 + hash_len + 1, 21, "%lu", timestamp);
		size_t timestamp_len = strlen(out_buf + action_len + 1 + hash_len + 1);

		out_buf[action_len + 1 + hash_len + 1 + timestamp_len] = '\n';

		int len = action_len + 1 + hash_len + 1 + timestamp_len + 1;


		pthread_mutex_lock(&ctx->seq_mutex);
		while (ctx->producer_rb->seq_control.id_next_nr != seq)
			pthread_cond_wait(&ctx->seq_cond, &ctx->seq_mutex);

		// save the log
		pthread_mutex_lock(&ctx->log_mutex);
		write(ctx->out_fd, out_buf, len);
		pthread_mutex_unlock(&ctx->log_mutex);

		ctx->producer_rb->seq_control.id_next_nr++;
		pthread_cond_broadcast(&ctx->seq_cond);
		pthread_mutex_unlock(&ctx->seq_mutex);
	}
	return NULL;
}

void init_ctx(so_consumer_ctx_t *ctx, so_ring_buffer_t *rb, int out_fd)
{
	ctx->producer_rb = rb;
	ctx->out_fd = out_fd;
	ctx->producer_rb->seq_control.id_next_nr = 0;
}

so_consumer_ctx_t *create_consumer_ctx(so_ring_buffer_t *rb, int out_fd)
{
	so_consumer_ctx_t *ctx = malloc(sizeof(so_consumer_ctx_t));

	if (ctx == NULL)
		return NULL;

	ctx->producer_rb = rb;
	ctx->out_fd = out_fd;
	ctx->producer_rb->seq_control.id_next_nr = 0;

	pthread_mutex_init(&ctx->log_mutex, NULL);
	pthread_mutex_init(&ctx->seq_mutex, NULL);
	pthread_cond_init(&ctx->seq_cond, NULL);

	return ctx;
}

int create_consumers(pthread_t *tids,
					 int num_consumers,
					 struct so_ring_buffer_t *rb,
					 const char *out_filename)
{
	int filed = open(out_filename, O_RDWR | O_CREAT | O_TRUNC, 0666);

	so_consumer_ctx_t *ctx = create_consumer_ctx(rb, open(out_filename, O_RDWR | O_CREAT | O_TRUNC, 0666));

	if (ctx == NULL) {
		close(filed);
		return -1;
	}

	if (filed < 0)
		return -1;

	for (int i = 0; i < num_consumers; i++)
		pthread_create(&tids[i], NULL, consumer_thread, ctx);

	return num_consumers;
}
