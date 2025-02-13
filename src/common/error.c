/*
 * Copyright (C) 2012 David Goulet <dgoulet@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#define _LGPL_SOURCE
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <common/common.h>
#include <common/thread.h>
#include <common/compat/errno.h>
#include <common/compat/getenv.h>
#include <lttng/lttng-error.h>

#include "error.h"

#define ERROR_INDEX(code) (code - LTTNG_OK)

/*
 * lttng_opt_abort_on_error: unset: -1, disabled: 0, enabled: 1.
 * Controlled by the LTTNG_ABORT_ON_ERROR environment variable.
 */
static int lttng_opt_abort_on_error = -1;

/* TLS variable that contains the time of one single log entry. */
DEFINE_URCU_TLS(struct log_time, error_log_time);
DEFINE_URCU_TLS(const char *, logger_thread_name);

LTTNG_HIDDEN
const char *log_add_time(void)
{
	int ret;
	struct tm tm, *res;
	struct timespec tp;
	time_t now;
	const int errsv = errno;

	ret = lttng_clock_gettime(CLOCK_REALTIME, &tp);
	if (ret < 0) {
		goto error;
	}
	now = (time_t) tp.tv_sec;

	res = localtime_r(&now, &tm);
	if (!res) {
		goto error;
	}

	/* Format time in the TLS variable. */
	ret = snprintf(URCU_TLS(error_log_time).str, sizeof(URCU_TLS(error_log_time).str),
			"%02d:%02d:%02d.%09ld",
			tm.tm_hour, tm.tm_min, tm.tm_sec, tp.tv_nsec);
	if (ret < 0) {
		goto error;
	}

	errno = errsv;
	return URCU_TLS(error_log_time).str;

error:
	/* Return an empty string on error so logging is not affected. */
	errno = errsv;
	return "";
}

LTTNG_HIDDEN
void logger_set_thread_name(const char *name, bool set_pthread_name)
{
	int ret;

	assert(name);
	URCU_TLS(logger_thread_name) = name;

	if (set_pthread_name) {
		ret = lttng_thread_setname(name);
		if (ret && ret != -ENOSYS) {
			/* Don't fail as this is not essential. */
			DBG("Failed to set pthread name attribute");
		}
	}
}

/*
 * Human readable error message.
 */
static const char *error_string_array[] = {
	/* LTTNG_OK code MUST be at the top for the ERROR_INDEX macro to work */
	[ ERROR_INDEX(LTTNG_OK) ] = "Success",
	[ ERROR_INDEX(LTTNG_ERR_UNK) ] = "Unknown error",
	[ ERROR_INDEX(LTTNG_ERR_UND) ] = "Undefined command",
	[ ERROR_INDEX(LTTNG_ERR_UNKNOWN_DOMAIN) ] = "Unknown tracing domain",
	[ ERROR_INDEX(LTTNG_ERR_NO_SESSION) ] = "No session found",
	[ ERROR_INDEX(LTTNG_ERR_CREATE_DIR_FAIL) ] = "Create directory failed",
	[ ERROR_INDEX(LTTNG_ERR_SESSION_FAIL) ] = "Create session failed",
	[ ERROR_INDEX(LTTNG_ERR_SESS_NOT_FOUND) ] = "Session name not found",
	[ ERROR_INDEX(LTTNG_ERR_FATAL) ] = "Fatal error of the session daemon",
	[ ERROR_INDEX(LTTNG_ERR_SELECT_SESS) ] = "A session MUST be selected",
	[ ERROR_INDEX(LTTNG_ERR_EXIST_SESS) ] = "Session name already exists",
	[ ERROR_INDEX(LTTNG_ERR_NO_EVENT) ] = "Event not found",
	[ ERROR_INDEX(LTTNG_ERR_CONNECT_FAIL) ] = "Unable to connect to Unix socket",
	[ ERROR_INDEX(LTTNG_ERR_EPERM) ] = "Permission denied",
	[ ERROR_INDEX(LTTNG_ERR_KERN_NA) ] = "Kernel tracer not available",
	[ ERROR_INDEX(LTTNG_ERR_KERN_VERSION) ] = "Kernel tracer version is not compatible",
	[ ERROR_INDEX(LTTNG_ERR_KERN_EVENT_EXIST) ] = "Kernel event already exists",
	[ ERROR_INDEX(LTTNG_ERR_KERN_SESS_FAIL) ] = "Kernel create session failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_CHAN_EXIST) ] = "Kernel channel already exists",
	[ ERROR_INDEX(LTTNG_ERR_KERN_CHAN_FAIL) ] = "Kernel create channel failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_CHAN_NOT_FOUND) ] = "Kernel channel not found",
	[ ERROR_INDEX(LTTNG_ERR_KERN_CHAN_DISABLE_FAIL) ] = "Disable kernel channel failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_CHAN_ENABLE_FAIL) ] = "Enable kernel channel failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_CONTEXT_FAIL) ] = "Add kernel context failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_ENABLE_FAIL) ] = "Enable kernel event failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_DISABLE_FAIL) ] = "Disable kernel event failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_META_FAIL) ] = "Opening metadata failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_START_FAIL) ] = "Starting kernel trace failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_STOP_FAIL) ] = "Stopping kernel trace failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_CONSUMER_FAIL) ] = "Kernel consumer start failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_STREAM_FAIL) ] = "Kernel create stream failed",
	[ ERROR_INDEX(LTTNG_ERR_KERN_LIST_FAIL) ] = "Listing kernel events failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_CALIBRATE_FAIL) ] = "UST calibration failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_SESS_FAIL) ] = "UST create session failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_CHAN_FAIL) ] = "UST create channel failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_CHAN_EXIST) ] = "UST channel already exist",
	[ ERROR_INDEX(LTTNG_ERR_UST_CHAN_NOT_FOUND) ] = "UST channel not found",
	[ ERROR_INDEX(LTTNG_ERR_UST_CHAN_DISABLE_FAIL) ] = "Disable UST channel failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_CHAN_ENABLE_FAIL) ] = "Enable UST channel failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_ENABLE_FAIL) ] = "Enable UST event failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_DISABLE_FAIL) ] = "Disable UST event failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_META_FAIL) ] = "Opening metadata failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_START_FAIL) ] = "Starting UST trace failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_STOP_FAIL) ] = "Stopping UST trace failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_CONSUMER64_FAIL) ] = "64-bit UST consumer start failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_CONSUMER32_FAIL) ] = "32-bit UST consumer start failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_STREAM_FAIL) ] = "UST create stream failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_LIST_FAIL) ] = "Listing UST events failed",
	[ ERROR_INDEX(LTTNG_ERR_UST_EVENT_EXIST) ] = "UST event already exist",
	[ ERROR_INDEX(LTTNG_ERR_UST_EVENT_NOT_FOUND)] = "UST event not found",
	[ ERROR_INDEX(LTTNG_ERR_UST_CONTEXT_EXIST)] = "UST context already exist",
	[ ERROR_INDEX(LTTNG_ERR_UST_CONTEXT_INVAL)] = "UST invalid context",
	[ ERROR_INDEX(LTTNG_ERR_NEED_ROOT_SESSIOND) ] = "Tracing the kernel requires a root lttng-sessiond daemon, as well as \"tracing\" group membership or root user ID for the lttng client.",
	[ ERROR_INDEX(LTTNG_ERR_NO_UST) ] = "LTTng-UST tracer is not supported. Please rebuild lttng-tools with lttng-ust support enabled.",
	[ ERROR_INDEX(LTTNG_ERR_TRACE_ALREADY_STARTED) ] = "Tracing has already been started once",
	[ ERROR_INDEX(LTTNG_ERR_TRACE_ALREADY_STOPPED) ] = "Tracing has already been stopped",
	[ ERROR_INDEX(LTTNG_ERR_KERN_EVENT_ENOSYS) ] = "Kernel event type not supported",
	[ ERROR_INDEX(LTTNG_ERR_NEED_CHANNEL_NAME) ] = "Non-default channel exists within session: channel name needs to be specified with '-c name'",
	[ ERROR_INDEX(LTTNG_ERR_INVALID) ] = "Invalid parameter",
	[ ERROR_INDEX(LTTNG_ERR_NO_USTCONSUMERD) ] = "No UST consumer detected",
	[ ERROR_INDEX(LTTNG_ERR_NO_KERNCONSUMERD) ] = "No kernel consumer detected",
	[ ERROR_INDEX(LTTNG_ERR_EVENT_EXIST_LOGLEVEL) ] = "Event already enabled with different loglevel",
	[ ERROR_INDEX(LTTNG_ERR_URL_DATA_MISS) ] = "Missing data path URL",
	[ ERROR_INDEX(LTTNG_ERR_URL_CTRL_MISS) ] = "Missing control path URL",
	[ ERROR_INDEX(LTTNG_ERR_ENABLE_CONSUMER_FAIL) ] = "Enabling consumer failed",
	[ ERROR_INDEX(LTTNG_ERR_RELAYD_CONNECT_FAIL) ] = "Unable to connect to lttng-relayd",
	[ ERROR_INDEX(LTTNG_ERR_RELAYD_VERSION_FAIL) ] = "Relay daemon not compatible",
	[ ERROR_INDEX(LTTNG_ERR_FILTER_INVAL) ] = "Invalid filter bytecode",
	[ ERROR_INDEX(LTTNG_ERR_FILTER_NOMEM) ] = "Not enough memory for filter bytecode",
	[ ERROR_INDEX(LTTNG_ERR_FILTER_EXIST) ] = "Filter already exist",
	[ ERROR_INDEX(LTTNG_ERR_NO_CONSUMER) ] = "Consumer not found for tracing session",
	[ ERROR_INDEX(LTTNG_ERR_NO_SESSIOND) ] = "No session daemon is available",
	[ ERROR_INDEX(LTTNG_ERR_SESSION_STARTED) ] = "Session is running",
	[ ERROR_INDEX(LTTNG_ERR_NOT_SUPPORTED) ] = "Operation not supported",
	[ ERROR_INDEX(LTTNG_ERR_UST_EVENT_ENABLED) ] = "UST event already enabled",
	[ ERROR_INDEX(LTTNG_ERR_SET_URL) ] = "Error setting URL",
	[ ERROR_INDEX(LTTNG_ERR_URL_EXIST) ] = "URL already exists",
	[ ERROR_INDEX(LTTNG_ERR_BUFFER_NOT_SUPPORTED)] = "Buffer type not supported",
	[ ERROR_INDEX(LTTNG_ERR_BUFFER_TYPE_MISMATCH)] = "Buffer type mismatch for session",
	[ ERROR_INDEX(LTTNG_ERR_NOMEM)] = "Not enough memory",
	[ ERROR_INDEX(LTTNG_ERR_SNAPSHOT_OUTPUT_EXIST) ] = "Snapshot output already exists",
	[ ERROR_INDEX(LTTNG_ERR_START_SESSION_ONCE) ] = "Session needs to be started once",
	[ ERROR_INDEX(LTTNG_ERR_SNAPSHOT_FAIL) ] = "Snapshot record failed",
	[ ERROR_INDEX(LTTNG_ERR_CHAN_EXIST) ] = "Channel already exists",
	[ ERROR_INDEX(LTTNG_ERR_SNAPSHOT_NODATA) ] = "No data available in snapshot",
	[ ERROR_INDEX(LTTNG_ERR_NO_CHANNEL) ] = "No channel found in the session",
	[ ERROR_INDEX(LTTNG_ERR_SESSION_INVALID_CHAR) ] = "Invalid character found in session name",
	[ ERROR_INDEX(LTTNG_ERR_SAVE_FILE_EXIST) ] = "Session file already exists",
	[ ERROR_INDEX(LTTNG_ERR_SAVE_IO_FAIL) ] = "IO error while writing session configuration",
	[ ERROR_INDEX(LTTNG_ERR_LOAD_INVALID_CONFIG) ] = "Invalid session configuration",
	[ ERROR_INDEX(LTTNG_ERR_LOAD_IO_FAIL) ] = "IO error while reading a session configuration",
	[ ERROR_INDEX(LTTNG_ERR_LOAD_SESSION_NOENT) ] = "Session file not found",
	[ ERROR_INDEX(LTTNG_ERR_MAX_SIZE_INVALID) ] = "Snapshot max size is invalid",
	[ ERROR_INDEX(LTTNG_ERR_MI_OUTPUT_TYPE) ] = "Invalid MI output format",
	[ ERROR_INDEX(LTTNG_ERR_MI_IO_FAIL) ] = "IO error while writing MI output",
	[ ERROR_INDEX(LTTNG_ERR_MI_NOT_IMPLEMENTED) ] = "Mi feature not implemented",
	[ ERROR_INDEX(LTTNG_ERR_INVALID_EVENT_NAME) ] = "Invalid event name",
	[ ERROR_INDEX(LTTNG_ERR_INVALID_CHANNEL_NAME) ] = "Invalid channel name",
	[ ERROR_INDEX(LTTNG_ERR_PROCESS_ATTR_EXISTS) ] = "Process attribute is already tracked",
	[ ERROR_INDEX(LTTNG_ERR_PROCESS_ATTR_MISSING) ] = "Process attribute was not tracked",
	[ ERROR_INDEX(LTTNG_ERR_INVALID_CHANNEL_DOMAIN) ] = "Invalid channel domain",
	[ ERROR_INDEX(LTTNG_ERR_OVERFLOW) ] = "Overflow occurred",
	[ ERROR_INDEX(LTTNG_ERR_SESSION_NOT_STARTED) ] = "Session not started",
	[ ERROR_INDEX(LTTNG_ERR_LIVE_SESSION) ] = "Live sessions are not supported",
	[ ERROR_INDEX(LTTNG_ERR_PER_PID_SESSION) ] = "Per-PID tracing sessions are not supported",
	[ ERROR_INDEX(LTTNG_ERR_KERN_CONTEXT_UNAVAILABLE) ] = "Context unavailable on this kernel",
	[ ERROR_INDEX(LTTNG_ERR_REGEN_STATEDUMP_FAIL) ] = "Failed to regenerate the state dump",
	[ ERROR_INDEX(LTTNG_ERR_REGEN_STATEDUMP_NOMEM) ] = "Failed to regenerate the state dump, not enough memory",
	[ ERROR_INDEX(LTTNG_ERR_NOT_SNAPSHOT_SESSION) ] = "Snapshot command can't be applied to a non-snapshot session",
	[ ERROR_INDEX(LTTNG_ERR_INVALID_TRIGGER) ] = "Invalid trigger",
	[ ERROR_INDEX(LTTNG_ERR_TRIGGER_EXISTS) ] = "Trigger already registered",
	[ ERROR_INDEX(LTTNG_ERR_TRIGGER_NOT_FOUND) ] = "Trigger not found",
	[ ERROR_INDEX(LTTNG_ERR_COMMAND_CANCELLED) ] = "Command cancelled",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_PENDING) ] = "Rotation already pending for this session",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_NOT_AVAILABLE) ] = "Rotation feature not available for this session's creation mode",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_SCHEDULE_SET) ] = "A session rotation schedule of this type is already set on the session",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_SCHEDULE_NOT_SET) ] = "No session rotation schedule of this type is set on the session",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_MULTIPLE_AFTER_STOP) ] = "Session was already rotated once since it became inactive",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_WRONG_VERSION) ] = "Session rotation is not supported by this kernel tracer version",
	[ ERROR_INDEX(LTTNG_ERR_NO_SESSION_OUTPUT) ] = "Session has no output",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_NOT_AVAILABLE_RELAY) ] = "Rotation feature not available on the relay",
	[ ERROR_INDEX(LTTNG_ERR_AGENT_TRACING_DISABLED) ] = "Session daemon agent tracing is disabled",
	[ ERROR_INDEX(LTTNG_ERR_PROBE_LOCATION_INVAL) ] = "Invalid userspace probe location",
	[ ERROR_INDEX(LTTNG_ERR_ELF_PARSING) ] = "ELF parsing error",
	[ ERROR_INDEX(LTTNG_ERR_SDT_PROBE_SEMAPHORE) ] = "SDT probe guarded by a semaphore",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_FAIL_CONSUMER) ] = "Rotation failure on consumer",
	[ ERROR_INDEX(LTTNG_ERR_ROTATE_RENAME_FAIL_CONSUMER) ] = "Rotation rename failure on consumer",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_PENDING_LOCAL_FAIL_CONSUMER) ] = "Rotation pending check (local) failure on consumer",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_PENDING_RELAY_FAIL_CONSUMER) ] = "Rotation pending check (relay) failure on consumer",
	[ ERROR_INDEX(LTTNG_ERR_MKDIR_FAIL_CONSUMER) ] = "Directory creation failure on consumer",
	[ ERROR_INDEX(LTTNG_ERR_CHAN_NOT_FOUND) ] = "Channel not found",
	[ ERROR_INDEX(LTTNG_ERR_SNAPSHOT_UNSUPPORTED) ] = "Session configuration does not allow the use of snapshots",
	[ ERROR_INDEX(LTTNG_ERR_SESSION_NOT_EXIST) ] = "Tracing session does not exist",
	[ ERROR_INDEX(LTTNG_ERR_CREATE_TRACE_CHUNK_FAIL_CONSUMER) ] = "Trace chunk creation failed on consumer",
	[ ERROR_INDEX(LTTNG_ERR_CLOSE_TRACE_CHUNK_FAIL_CONSUMER) ] = "Trace chunk close failed on consumer",
	[ ERROR_INDEX(LTTNG_ERR_TRACE_CHUNK_EXISTS_FAIL_CONSUMER) ] = "Failed to query consumer for trace chunk existence",
	[ ERROR_INDEX(LTTNG_ERR_INVALID_PROTOCOL) ] = "Protocol error occurred",
	[ ERROR_INDEX(LTTNG_ERR_FILE_CREATION_ERROR) ] = "Failed to create file",
	[ ERROR_INDEX(LTTNG_ERR_TIMER_STOP_ERROR) ] = "Failed to stop a timer",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_NOT_AVAILABLE_KERNEL) ] = "Rotation feature not supported by the kernel tracer.",
	[ ERROR_INDEX(LTTNG_ERR_CLEAR_RELAY_DISALLOWED) ] = "Relayd daemon peer does not allow sessions to be cleared",
	[ ERROR_INDEX(LTTNG_ERR_CLEAR_NOT_AVAILABLE_RELAY) ] = "Clearing a session is not supported by the relay daemon",
	[ ERROR_INDEX(LTTNG_ERR_CLEAR_FAIL_CONSUMER) ] = "Consumer failed to clear the session",
	[ ERROR_INDEX(LTTNG_ERR_ROTATION_AFTER_STOP_CLEAR) ] = "Session was already cleared since it became inactive",
	[ ERROR_INDEX(LTTNG_ERR_USER_NOT_FOUND) ] = "User not found",
	[ ERROR_INDEX(LTTNG_ERR_GROUP_NOT_FOUND) ] = "Group not found",
	[ ERROR_INDEX(LTTNG_ERR_UNSUPPORTED_DOMAIN) ] = "Unsupported domain used",
	[ ERROR_INDEX(LTTNG_ERR_PROCESS_ATTR_TRACKER_INVALID_TRACKING_POLICY) ] = "Operation does not apply to the process attribute tracker's tracking policy",
	[ ERROR_INDEX(LTTNG_ERR_EVENT_NOTIFIER_GROUP_NOTIFICATION_FD) ] = "Failed to create an event notifier group notification file descriptor",
	[ ERROR_INDEX(LTTNG_ERR_INVALID_CAPTURE_EXPRESSION) ] = "Invalid capture expression",
	[ ERROR_INDEX(LTTNG_ERR_EVENT_NOTIFIER_REGISTRATION) ] = "Failed to create event notifier",
	[ ERROR_INDEX(LTTNG_ERR_EVENT_NOTIFIER_ERROR_ACCOUNTING) ] = "Failed to initialize event notifier error accounting",
	[ ERROR_INDEX(LTTNG_ERR_EVENT_NOTIFIER_ERROR_ACCOUNTING_FULL) ] = "No index available in event notifier error accounting",

	/* Last element */
	[ ERROR_INDEX(LTTNG_ERR_NR) ] = "Unknown error code"
};

/*
 * Return ptr to string representing a human readable error code from the
 * lttng_error_code enum.
 *
 * These code MUST be negative in other to treat that as an error value.
 */
LTTNG_HIDDEN
const char *error_get_str(int32_t code)
{
	code = -code;

	if (code < LTTNG_OK || code > LTTNG_ERR_NR) {
		code = LTTNG_ERR_NR;
	}

	return error_string_array[ERROR_INDEX(code)];
}

LTTNG_HIDDEN
void lttng_abort_on_error(void)
{
	if (lttng_opt_abort_on_error < 0) {
		/* Use lttng_secure_getenv() to query its state. */
		const char *value;

		value = lttng_secure_getenv("LTTNG_ABORT_ON_ERROR");
		if (value && !strcmp(value, "1")) {
			lttng_opt_abort_on_error = 1;
		} else {
			lttng_opt_abort_on_error = 0;
		}
	}
	if (lttng_opt_abort_on_error > 0) {
		abort();
	}
}
