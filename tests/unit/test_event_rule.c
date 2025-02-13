/*
 * Unit tests for the notification API.
 *
 * Copyright (C) 2019 Jonathan Rajotte <jonathan.rajotte-julien@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <tap/tap.h>

#include <common/payload-view.h>
#include <common/payload.h>
#include <lttng/domain.h>
#include <lttng/event-rule/kernel-probe-internal.h>
#include <lttng/event-rule/kernel-probe.h>
#include <lttng/event-rule/syscall-internal.h>
#include <lttng/event-rule/syscall.h>
#include <lttng/event-rule/tracepoint-internal.h>
#include <lttng/event-rule/tracepoint.h>
#include <lttng/event-rule/userspace-probe-internal.h>
#include <lttng/event-rule/userspace-probe.h>
#include <lttng/event.h>
#include <lttng/kernel-probe-internal.h>
#include <lttng/kernel-probe.h>
#include <lttng/userspace-probe-internal.h>
#include <lttng/userspace-probe.h>
#include "bin/lttng/loglevel.h"

/* For error.h. */
int lttng_opt_quiet = 1;
int lttng_opt_verbose;
int lttng_opt_mi;

#define NUM_TESTS 246

struct tracepoint_test {
	enum lttng_domain_type type;
	bool support_exclusion;
};

typedef const char *(*log_level_name_getter)(int log_level);

static
void test_event_rule_tracepoint_by_domain(const struct tracepoint_test *test)
{
	unsigned int count;
	struct lttng_event_rule *tracepoint = NULL;
	struct lttng_event_rule *tracepoint_from_buffer = NULL;
	enum lttng_event_rule_status status;
	enum lttng_domain_type domain_type, type;
	const char *pattern="my_event_*";
	const char *filter="msg_id == 23 && size >= 2048";
	const char *tmp;
	const char *exclusions[] = {"my_event_test1", "my_event_test2" ,"my_event_test3"};
	struct lttng_log_level_rule *log_level_rule = NULL;
	const struct lttng_log_level_rule *log_level_rule_return = NULL;
	struct lttng_payload payload;

	type = test->type;
	diag("Testing domain %d.", type);

	lttng_payload_init(&payload);

	log_level_rule = lttng_log_level_rule_exactly_create(LTTNG_LOGLEVEL_INFO);
	assert(log_level_rule);

	tracepoint = lttng_event_rule_tracepoint_create(type);
	ok(tracepoint, "tracepoint object.");

	status = lttng_event_rule_tracepoint_get_domain_type(tracepoint, &domain_type);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "get tracepoint domain.");
	ok(domain_type == type, "domain type got %d expected %d.", domain_type, type);

	status = lttng_event_rule_tracepoint_set_pattern(tracepoint, pattern);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "setting pattern.");
	status = lttng_event_rule_tracepoint_get_pattern(tracepoint, &tmp);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "getting pattern.");
	ok(!strncmp(pattern, tmp, strlen(pattern)), "pattern is equal.");

	status = lttng_event_rule_tracepoint_set_filter(tracepoint, filter);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "setting filter.");
	status = lttng_event_rule_tracepoint_get_filter(tracepoint, &tmp);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "getting filter.");
	ok(!strncmp(filter, tmp, strlen(filter)), "filter is equal.");

	status = lttng_event_rule_tracepoint_get_log_level_rule(tracepoint, &log_level_rule_return);
	ok(status == LTTNG_EVENT_RULE_STATUS_UNSET, "get unset log level rule.");

	if (type != LTTNG_DOMAIN_KERNEL) {
		status = lttng_event_rule_tracepoint_set_log_level_rule(tracepoint, log_level_rule);
		ok(status == LTTNG_EVENT_RULE_STATUS_OK, "setting log level rule.");
		status = lttng_event_rule_tracepoint_get_log_level_rule(tracepoint, &log_level_rule_return);
		ok(status == LTTNG_EVENT_RULE_STATUS_OK, "get log level rule.");
	}

	if (test->support_exclusion) {
		int i;

		for (i = 0; i < 3; i++) {
			status = lttng_event_rule_tracepoint_add_exclusion(tracepoint, exclusions[i]);
			ok(status == LTTNG_EVENT_RULE_STATUS_OK, "setting exclusions \"%s\"", exclusions[i]);
		}

		status = lttng_event_rule_tracepoint_get_exclusions_count(tracepoint, &count);
		ok(status == LTTNG_EVENT_RULE_STATUS_OK, "getting exclusion count.");
		ok(count == 3, "count is %d/3", count);

		for (i = 0; i < count; i++) {
			status = lttng_event_rule_tracepoint_get_exclusion_at_index(tracepoint, i, &tmp);
			ok(status == LTTNG_EVENT_RULE_STATUS_OK, "getting exclusion at index %d.", i);
			ok(!strncmp(exclusions[i], tmp, strlen(exclusions[i])), "%s == %s.", tmp, exclusions[i]);
		}
	} else {
		int i;

		for (i = 0; i < 3; i++) {
			status = lttng_event_rule_tracepoint_add_exclusion(tracepoint, exclusions[i]);
			ok(status == LTTNG_EVENT_RULE_STATUS_UNSUPPORTED, "setting exclusions unsupported \"%s\".", exclusions[i]);
		}

		status = lttng_event_rule_tracepoint_get_exclusions_count(tracepoint, &count);
		ok(status == LTTNG_EVENT_RULE_STATUS_OK, "getting exclusion count.");
		ok(count == 0, "count is %d/0", count);
	}

	ok(lttng_event_rule_serialize(tracepoint, &payload) == 0, "Serializing.");

	{
		struct lttng_payload_view view =
				lttng_payload_view_from_payload(
						&payload, 0, -1);

		ok(lttng_event_rule_create_from_payload(
				&view, &tracepoint_from_buffer) > 0,
				"Deserializing.");
	}

	ok(lttng_event_rule_is_equal(tracepoint, tracepoint_from_buffer), "serialized and from buffer are equal.");

	lttng_payload_reset(&payload);
	lttng_event_rule_destroy(tracepoint);
	lttng_event_rule_destroy(tracepoint_from_buffer);
	lttng_log_level_rule_destroy(log_level_rule);
}

static
void test_event_rule_tracepoint(void)
{
	int i;
	struct lttng_event_rule *tracepoint = NULL;
	struct tracepoint_test tests[] = {{LTTNG_DOMAIN_JUL, false},
			{LTTNG_DOMAIN_KERNEL, false},
			{LTTNG_DOMAIN_LOG4J, false},
			{LTTNG_DOMAIN_PYTHON, false},
			{LTTNG_DOMAIN_UST, true}};

	diag("Testing lttng_event_rule_tracepoint.");
	tracepoint = lttng_event_rule_tracepoint_create(LTTNG_DOMAIN_NONE);
	ok(!tracepoint, "Domain type restriction on create.");

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		test_event_rule_tracepoint_by_domain(&tests[i]);
	}
}

static void test_event_rule_syscall(void)
{
	struct lttng_event_rule *syscall = NULL;
	struct lttng_event_rule *syscall_from_buffer = NULL;
	enum lttng_event_rule_status status;
	const char *pattern = "my_event_*";
	const char *filter = "msg_id == 23 && size >= 2048";
	const char *tmp;
	struct lttng_payload payload;

	diag("Event rule syscall.");

	lttng_payload_init(&payload);

	syscall = lttng_event_rule_syscall_create();
	ok(syscall, "syscall object.");

	status = lttng_event_rule_syscall_set_pattern(syscall, pattern);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "setting pattern.");
	status = lttng_event_rule_syscall_get_pattern(syscall, &tmp);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "getting pattern.");
	ok(!strncmp(pattern, tmp, strlen(pattern)), "pattern is equal.");

	status = lttng_event_rule_syscall_set_filter(syscall, filter);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "setting filter.");
	status = lttng_event_rule_syscall_get_filter(syscall, &tmp);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "getting filter.");
	ok(!strncmp(filter, tmp, strlen(filter)), "filter is equal.");

	ok(lttng_event_rule_serialize(syscall, &payload) == 0, "Serializing.");

	{
		struct lttng_payload_view view =
				lttng_payload_view_from_payload(
						&payload, 0, -1);

		ok(lttng_event_rule_create_from_payload(
				   &view, &syscall_from_buffer) > 0,
				"Deserializing.");
	}

	ok(lttng_event_rule_is_equal(syscall, syscall_from_buffer),
			"serialized and from buffer are equal.");

	lttng_payload_reset(&payload);
	lttng_event_rule_destroy(syscall);
	lttng_event_rule_destroy(syscall_from_buffer);
}

static void test_event_rule_userspace_probe(void)
{
	struct lttng_event_rule *uprobe = NULL;
	struct lttng_event_rule *uprobe_from_buffer = NULL;
	struct lttng_userspace_probe_location_lookup_method *lookup_method =
			NULL;
	struct lttng_userspace_probe_location *probe_location = NULL;
	const struct lttng_userspace_probe_location *probe_location_tmp = NULL;
	enum lttng_event_rule_status status;

	const char *probe_name = "my_probe.";
	const char *tmp;
	struct lttng_payload payload;

	diag("Event rule uprobe.");

	lookup_method = lttng_userspace_probe_location_lookup_method_function_elf_create();
	if (!lookup_method) {
		fail("Setup error on userspace probe lookup method creation.");
		goto end;
	}

	probe_location = lttng_userspace_probe_location_function_create(
			"/proc/self/exe",
			"lttng_userspace_probe_location_tracepoint_create",
			lookup_method);
	if (!probe_location) {
		fail("Setup error on userspace probe location creation.");
		goto end;
	}

	/* Ownership transferred to the probe location function object. */
	lookup_method = NULL;

	lttng_payload_init(&payload);

	uprobe = lttng_event_rule_userspace_probe_create(probe_location);
	ok(uprobe, "uprobe event rule object creation.");

	status = lttng_event_rule_userspace_probe_get_location(
			uprobe, &probe_location_tmp);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK,
			"Getting uprobe event rule location.");
	ok(lttng_userspace_probe_location_is_equal(
			   probe_location, probe_location_tmp),
			"Location is equal.");

	status = lttng_event_rule_userspace_probe_set_event_name(uprobe, probe_name);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK,
			"Setting uprobe event rule name: %s.", probe_name);
	status = lttng_event_rule_userspace_probe_get_event_name(uprobe, &tmp);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "Getting uprobe name.");
	ok(!strcmp(probe_name, tmp), "Uprobe name are equal.");

	ok(lttng_event_rule_serialize(uprobe, &payload) == 0, "Serializing.");

	{
		struct lttng_payload_view view =
				lttng_payload_view_from_payload(
						&payload, 0, -1);

		ok(lttng_event_rule_create_from_payload(
				   &view, &uprobe_from_buffer) > 0,
				"Deserializing.");
	}

	ok(lttng_event_rule_is_equal(uprobe, uprobe_from_buffer),
			"serialized and from buffer are equal.");

end:
	lttng_payload_reset(&payload);
	lttng_event_rule_destroy(uprobe);
	lttng_event_rule_destroy(uprobe_from_buffer);
	lttng_userspace_probe_location_destroy(probe_location);
	lttng_userspace_probe_location_lookup_method_destroy(lookup_method);
}

static void test_event_rule_kernel_probe_by_location(
		const struct lttng_kernel_probe_location *location)
{
	struct lttng_event_rule *kprobe = NULL;
	struct lttng_event_rule *kprobe_from_buffer = NULL;
	enum lttng_event_rule_status status;
	const struct lttng_kernel_probe_location *_location;

	const char *probe_name = "my_probe";
	const char *tmp;
	struct lttng_payload payload;

	diag("Event rule kprobe for location type %d.",
			lttng_kernel_probe_location_get_type(location));

	lttng_payload_init(&payload);

	kprobe = lttng_event_rule_kernel_probe_create(location);
	ok(kprobe, "kprobe event rule object creation.");

	status = lttng_event_rule_kernel_probe_get_location(kprobe, &_location);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK,
			"Getting kprobe event rule location.");
	ok(lttng_kernel_probe_location_is_equal(location, _location), "Locations are equal.");

	status = lttng_event_rule_kernel_probe_set_event_name(kprobe, probe_name);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK,
			"Setting kprobe event rule name: %s.", probe_name);
	status = lttng_event_rule_kernel_probe_get_event_name(kprobe, &tmp);
	ok(status == LTTNG_EVENT_RULE_STATUS_OK, "Getting kprobe name.");
	ok(!strcmp(probe_name, tmp), "kprobe name are equal.");

	ok(lttng_event_rule_serialize(kprobe, &payload) == 0, "Serializing.");

	{
		struct lttng_payload_view view =
				lttng_payload_view_from_payload(
						&payload, 0, -1);

		ok(lttng_event_rule_create_from_payload(
				   &view, &kprobe_from_buffer) > 0,
				"Deserializing.");
	}

	ok(lttng_event_rule_is_equal(kprobe, kprobe_from_buffer),
			"serialized and from buffer are equal.");

	lttng_payload_reset(&payload);
	lttng_event_rule_destroy(kprobe);
	lttng_event_rule_destroy(kprobe_from_buffer);
}

static void test_event_rule_kernel_probe(void)
{
	struct lttng_kernel_probe_location *address_location = NULL;
	struct lttng_kernel_probe_location *symbol_location = NULL;

	address_location = lttng_kernel_probe_location_address_create(50);
	symbol_location = lttng_kernel_probe_location_symbol_create("une_bonne", 50);
	assert(address_location);
	assert(symbol_location);

	test_event_rule_kernel_probe_by_location(address_location);
	test_event_rule_kernel_probe_by_location(symbol_location);

	lttng_kernel_probe_location_destroy(address_location);
	lttng_kernel_probe_location_destroy(symbol_location);
}

static void test_set_event_rule_log_level_rules(
		struct lttng_event_rule *event_rule,
		int log_level,
		enum lttng_event_rule_status *exactly_status,
		enum lttng_event_rule_status *as_severe_status)
{
	struct lttng_log_level_rule *log_level_rule;

	log_level_rule = lttng_log_level_rule_at_least_as_severe_as_create(
			log_level);
	assert(log_level_rule);

	*as_severe_status = lttng_event_rule_tracepoint_set_log_level_rule(
			event_rule, log_level_rule);
	lttng_log_level_rule_destroy(log_level_rule);

	log_level_rule = lttng_log_level_rule_exactly_create(log_level);
	assert(log_level_rule);

	*exactly_status = lttng_event_rule_tracepoint_set_log_level_rule(
			event_rule, log_level_rule);
	lttng_log_level_rule_destroy(log_level_rule);
}

static void test_event_rule_log_level_kernel(void)
{
	struct lttng_event_rule *kernel_tracepoint_rule;
	enum lttng_event_rule_status er_exactly_status, er_as_severe_status;

	diag("Test kernel event rule + log level rule");
	kernel_tracepoint_rule =
			lttng_event_rule_tracepoint_create(LTTNG_DOMAIN_KERNEL);
	assert(kernel_tracepoint_rule);

	test_set_event_rule_log_level_rules(kernel_tracepoint_rule, 0, &er_exactly_status, &er_as_severe_status);
	ok(er_exactly_status == LTTNG_EVENT_RULE_STATUS_UNSUPPORTED,
			"Log level rule \"exactly\" rejected by kernel tracepoint event rule (unsupported)");
	ok(er_as_severe_status == LTTNG_EVENT_RULE_STATUS_UNSUPPORTED,
			"Log level rule \"at least as severe as\" rejected by kernel tracepoint event rule (unsupported)");

	lttng_event_rule_destroy(kernel_tracepoint_rule);
}

static void test_event_rule_log_level_generic(const char *domain_name,
		enum lttng_domain_type domain,
		log_level_name_getter get_log_level_name,
		const int tagged_log_level_values[],
		size_t tagged_log_level_values_count,
		const int valid_log_level_values[],
		size_t valid_log_level_values_count,
		const int invalid_log_level_values[],
		size_t invalid_log_level_values_count)
{
	size_t i;
	struct lttng_event_rule *tracepoint_rule;
	enum lttng_event_rule_status er_exactly_status, er_as_severe_status;

	diag("Test %s event rule + log level rule", domain_name);
	tracepoint_rule = lttng_event_rule_tracepoint_create(domain);
	assert(tracepoint_rule);

	for (i = 0; i < tagged_log_level_values_count; i++) {
		const int tagged_log_level_value = tagged_log_level_values[i];

		test_set_event_rule_log_level_rules(tracepoint_rule,
				tagged_log_level_value,
				&er_exactly_status, &er_as_severe_status);
		ok(er_exactly_status == LTTNG_EVENT_RULE_STATUS_OK,
				"Log level rule \"exactly\" accepted by %s tracepoint event rule: level = %s",
				domain_name,
				get_log_level_name(
						tagged_log_level_value));
		ok(er_as_severe_status == LTTNG_EVENT_RULE_STATUS_OK,
				"Log level rule \"as least as severe as\" accepted by %s tracepoint event rule: level = %s",
				domain_name,
				get_log_level_name(
						tagged_log_level_value));
	}

	for (i = 0; i < valid_log_level_values_count; i++) {
		const int valid_log_level_value = valid_log_level_values[i];

		test_set_event_rule_log_level_rules(tracepoint_rule,
				valid_log_level_value,
				&er_exactly_status, &er_as_severe_status);
		ok(er_exactly_status == LTTNG_EVENT_RULE_STATUS_OK,
				"Log level rule \"exactly\" accepted by %s tracepoint event rule: level = %d",
				domain_name,
				valid_log_level_value);
		ok(er_as_severe_status == LTTNG_EVENT_RULE_STATUS_OK,
				"Log level rule \"as least as severe as\" accepted by %s tracepoint event rule: level = %d",
				domain_name,
				valid_log_level_value);
	}

	for (i = 0; i < invalid_log_level_values_count; i++) {
		const int invalid_log_level_value = invalid_log_level_values[i];

		test_set_event_rule_log_level_rules(tracepoint_rule,
				invalid_log_level_value,
				&er_exactly_status, &er_as_severe_status);
		ok(er_exactly_status == LTTNG_EVENT_RULE_STATUS_INVALID,
				"Log level rule \"exactly\" rejected by %s tracepoint event rule: level = %d",
				domain_name,
				invalid_log_level_value);
		ok(er_as_severe_status == LTTNG_EVENT_RULE_STATUS_INVALID,
				"Log level rule \"as least as severe as\" rejected by %s tracepoint event rule: level = %d",
				domain_name,
				invalid_log_level_value);
	}

	lttng_event_rule_destroy(tracepoint_rule);
}

static void test_event_rule_log_level_ust(void)
{
	const int tagged_log_level_values[] = {
		LTTNG_LOGLEVEL_EMERG,
		LTTNG_LOGLEVEL_ALERT,
		LTTNG_LOGLEVEL_CRIT,
		LTTNG_LOGLEVEL_ERR,
		LTTNG_LOGLEVEL_WARNING,
		LTTNG_LOGLEVEL_NOTICE,
		LTTNG_LOGLEVEL_INFO,
		LTTNG_LOGLEVEL_DEBUG_SYSTEM,
		LTTNG_LOGLEVEL_DEBUG_PROGRAM,
		LTTNG_LOGLEVEL_DEBUG_PROCESS,
		LTTNG_LOGLEVEL_DEBUG_MODULE,
		LTTNG_LOGLEVEL_DEBUG_UNIT,
		LTTNG_LOGLEVEL_DEBUG_FUNCTION,
		LTTNG_LOGLEVEL_DEBUG_LINE,
		LTTNG_LOGLEVEL_DEBUG,
	};
	const int invalid_log_level_values[] = {
		-1980,
		1995,
		LTTNG_LOGLEVEL_DEBUG + 1,
		LTTNG_LOGLEVEL_EMERG - 1,
	};

	test_event_rule_log_level_generic("user space", LTTNG_DOMAIN_UST,
			loglevel_value_to_name, tagged_log_level_values,
			ARRAY_SIZE(tagged_log_level_values),
			NULL, 0,
			invalid_log_level_values,
			ARRAY_SIZE(invalid_log_level_values));
}

static void test_event_rule_log_level_jul(void)
{
	const int tagged_log_level_values[] = {
		LTTNG_LOGLEVEL_JUL_OFF,
		LTTNG_LOGLEVEL_JUL_SEVERE,
		LTTNG_LOGLEVEL_JUL_WARNING,
		LTTNG_LOGLEVEL_JUL_INFO,
		LTTNG_LOGLEVEL_JUL_CONFIG,
		LTTNG_LOGLEVEL_JUL_FINE,
		LTTNG_LOGLEVEL_JUL_FINER,
		LTTNG_LOGLEVEL_JUL_FINEST,
		LTTNG_LOGLEVEL_JUL_ALL,
	};
	const int valid_log_level_values[] = {
		0,
		-1980,
		1995
	};

	test_event_rule_log_level_generic("Java Util Logging", LTTNG_DOMAIN_JUL,
			loglevel_jul_value_to_name, tagged_log_level_values,
			ARRAY_SIZE(tagged_log_level_values),
			valid_log_level_values,
			ARRAY_SIZE(valid_log_level_values), NULL, 0);
}

static void test_event_rule_log_level_log4j(void)
{
	const int tagged_log_level_values[] = {
		LTTNG_LOGLEVEL_LOG4J_OFF,
		LTTNG_LOGLEVEL_LOG4J_FATAL,
		LTTNG_LOGLEVEL_LOG4J_ERROR,
		LTTNG_LOGLEVEL_LOG4J_WARN,
		LTTNG_LOGLEVEL_LOG4J_INFO,
		LTTNG_LOGLEVEL_LOG4J_DEBUG,
		LTTNG_LOGLEVEL_LOG4J_TRACE,
		LTTNG_LOGLEVEL_LOG4J_ALL,
	};
	const int valid_log_level_values[] = {
		0
		-1980,
		1995
	};

	test_event_rule_log_level_generic("Log4j", LTTNG_DOMAIN_LOG4J,
			loglevel_log4j_value_to_name, tagged_log_level_values,
			ARRAY_SIZE(tagged_log_level_values),
			valid_log_level_values,
			ARRAY_SIZE(valid_log_level_values), NULL, 0);
}

static void test_event_rule_log_level_python(void)
{
	const int tagged_log_level_values[] = {
		LTTNG_LOGLEVEL_PYTHON_CRITICAL,
		LTTNG_LOGLEVEL_PYTHON_ERROR,
		LTTNG_LOGLEVEL_PYTHON_WARNING,
		LTTNG_LOGLEVEL_PYTHON_INFO,
		LTTNG_LOGLEVEL_PYTHON_DEBUG,
		LTTNG_LOGLEVEL_PYTHON_NOTSET,
	};
	const int valid_log_level_values[] = {
		45,
		35,
		0,
		-657,
	};

	test_event_rule_log_level_generic("Python", LTTNG_DOMAIN_PYTHON,
			loglevel_python_value_to_name, tagged_log_level_values,
			ARRAY_SIZE(tagged_log_level_values),
			valid_log_level_values,
			ARRAY_SIZE(valid_log_level_values),
			NULL, 0);
}

int main(int argc, const char *argv[])
{
	plan_tests(NUM_TESTS);
	test_event_rule_tracepoint();
	test_event_rule_syscall();
	test_event_rule_userspace_probe();
	test_event_rule_kernel_probe();
	test_event_rule_log_level_kernel();
	test_event_rule_log_level_ust();
	test_event_rule_log_level_jul();
	test_event_rule_log_level_log4j();
	test_event_rule_log_level_python();
	return exit_status();
}
