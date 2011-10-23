/*
 * Copyright (C) 2011 - David Goulet <david.goulet@polymtl.ca>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; only version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _LTT_TRACE_KERNEL_H
#define _LTT_TRACE_KERNEL_H

#include <limits.h>
#include <urcu/list.h>

#include <lttng/lttng.h>
#include <lttng-kernel.h>

/* Kernel event list */
struct ltt_kernel_event_list {
	struct cds_list_head head;
};

/* Channel stream list */
struct ltt_kernel_stream_list {
	struct cds_list_head head;
};

/* Channel list */
struct ltt_kernel_channel_list {
	struct cds_list_head head;
};

/* Kernel event */
struct ltt_kernel_event {
	int fd;
	int enabled;
	struct lttng_kernel_context *ctx;
	struct lttng_kernel_event *event;
	struct cds_list_head list;
};

/* Kernel channel */
struct ltt_kernel_channel {
	int fd;
	int enabled;
	char *pathname;
	unsigned int stream_count;
	unsigned int event_count;
	struct lttng_kernel_context *ctx;
	struct lttng_channel *channel;
	struct ltt_kernel_event_list events_list;
	struct ltt_kernel_stream_list stream_list;
	struct cds_list_head list;
};

/* Metadata */
struct ltt_kernel_metadata {
	int fd;
	char *pathname;
	struct lttng_channel *conf;
};

/* Channel stream */
struct ltt_kernel_stream {
	int fd;
	char *pathname;
	int state;
	struct cds_list_head list;
};

/* Kernel session */
struct ltt_kernel_session {
	int fd;
	int metadata_stream_fd;
	int consumer_fds_sent;
	int consumer_fd;
	unsigned int channel_count;
	unsigned int stream_count_global;
	char *trace_path;
	struct ltt_kernel_metadata *metadata;
	struct ltt_kernel_channel_list channel_list;
};

/*
 * Lookup functions. NULL is returned if not found.
 */
struct ltt_kernel_event *trace_kernel_get_event_by_name(
		char *name, struct ltt_kernel_channel *channel);
struct ltt_kernel_channel *trace_kernel_get_channel_by_name(
		char *name, struct ltt_kernel_session *session);

/*
 * Create functions malloc() the data structure.
 */
struct ltt_kernel_session *trace_kernel_create_session(char *path);
struct ltt_kernel_channel *trace_kernel_create_channel(struct lttng_channel *chan, char *path);
struct ltt_kernel_event *trace_kernel_create_event(struct lttng_event *ev);
struct ltt_kernel_metadata *trace_kernel_create_metadata(char *path);
struct ltt_kernel_stream *trace_kernel_create_stream(void);

/*
 * Destroy functions free() the data structure and remove from linked list if
 * it's applies.
 */
void trace_kernel_destroy_session(struct ltt_kernel_session *session);
void trace_kernel_destroy_metadata(struct ltt_kernel_metadata *metadata);
void trace_kernel_destroy_channel(struct ltt_kernel_channel *channel);
void trace_kernel_destroy_event(struct ltt_kernel_event *event);
void trace_kernel_destroy_stream(struct ltt_kernel_stream *stream);

#endif /* _LTT_TRACE_KERNEL_H */
