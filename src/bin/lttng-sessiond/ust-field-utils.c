/*
 * Copyright (C) 2018 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include <stdbool.h>
#include <string.h>

#include "ust-field-utils.h"

/*
 * The ustctl_field is made of a combination of C basic types
 * ustctl_basic_type and _ustctl_basic_type.
 *
 * ustctl_basic_type contains an enumeration describing the abstract type.
 * _ustctl_basic_type does _NOT_ contain an enumeration describing the
 * abstract type.
 *
 * A layer is needed to use the same code for both structures.
 * When dealing with _ustctl_basic_type, we need to use the abstract type of
 * the ustctl_type struct.
 */

/*
 * Compare two ustctl_integer_type fields.
 * Returns 1 if both are identical.
 */
static bool match_ustctl_field_integer(const struct ustctl_integer_type *first,
			const struct ustctl_integer_type *second)
{
	if (first->size != second->size) {
		goto no_match;
	}
	if (first->alignment != second->alignment) {
		goto no_match;
	}
	if (first->signedness != second->signedness) {
		goto no_match;
	}
	if (first->encoding != second->encoding) {
		goto no_match;
	}
	if (first->base != second->base) {
		goto no_match;
	}
	if (first->reverse_byte_order != second->reverse_byte_order) {
		goto no_match;
	}

	return true;

no_match:
	return false;
}

/*
 * Compare two _ustctl_basic_type fields known to be of type integer.
 * Returns 1 if both are identical.
 */
static bool match_ustctl_field_integer_from_raw_basic_type(
			const union _ustctl_basic_type *first,
			const union _ustctl_basic_type *second)
{
	return match_ustctl_field_integer(&first->integer, &second->integer);
}

/*
 * Compare two _ustctl_basic_type fields known to be of type enum.
 * Returns 1 if both are identical.
 */
static bool match_ustctl_field_enum_from_raw_basic_type(
		const union _ustctl_basic_type *first,
		const union _ustctl_basic_type *second)
{
	/*
	 * Compare enumeration ID. Enumeration ID is provided to the application by
	 * the session daemon before event registration.
	 */
	if (first->enumeration.id != second->enumeration.id) {
		goto no_match;
	}

	/*
	 * Sanity check of the name and container type. Those were already checked
	 * during enum registration.
	 */
	if (strncmp(first->enumeration.name, second->enumeration.name,
				LTTNG_UST_ABI_SYM_NAME_LEN)) {
		goto no_match;
	}
	if (!match_ustctl_field_integer(&first->enumeration.container_type,
				&second->enumeration.container_type)) {
		goto no_match;
	}

	return true;

no_match:
	return false;
}

/*
 * Compare two _ustctl_basic_type fields known to be of type string.
 * Returns 1 if both are identical.
 */
static bool match_ustctl_field_string_from_raw_basic_type(
		const union _ustctl_basic_type *first,
		const union _ustctl_basic_type *second)
{
	return first->string.encoding == second->string.encoding;
}

/*
 * Compare two _ustctl_basic_type fields known to be of type float.
 * Returns 1 if both are identical.
 */
static bool match_ustctl_field_float_from_raw_basic_type(
		const union _ustctl_basic_type *first,
		const union _ustctl_basic_type *second)
{
	if (first->_float.exp_dig != second->_float.exp_dig) {
		goto no_match;
	}

	if (first->_float.mant_dig != second->_float.mant_dig) {
		goto no_match;
	}

	if (first->_float.reverse_byte_order !=
			second->_float.reverse_byte_order) {
		goto no_match;
	}

	if (first->_float.alignment != second->_float.alignment) {
		goto no_match;
	}

	return true;

no_match:
	return false;
}

/*
 * Compare two _ustctl_basic_type fields given their respective abstract types.
 * Returns 1 if both are identical.
 */
static bool match_ustctl_field_raw_basic_type(
		enum ustctl_abstract_types first_atype,
		const union _ustctl_basic_type *first,
		enum ustctl_abstract_types second_atype,
		const union _ustctl_basic_type *second)
{
	if (first_atype != second_atype) {
		goto no_match;
	}

	switch (first_atype) {
	case ustctl_atype_integer:
		if (!match_ustctl_field_integer_from_raw_basic_type(first, second)) {
			goto no_match;
		}
		break;
	case ustctl_atype_enum:
		if (!match_ustctl_field_enum_from_raw_basic_type(first, second)) {
			goto no_match;
		}
		break;
	case ustctl_atype_string:
		if (!match_ustctl_field_string_from_raw_basic_type(first, second)) {
			goto no_match;
		}
		break;
	case ustctl_atype_float:
		if (!match_ustctl_field_float_from_raw_basic_type(first, second)) {
			goto no_match;
		}
		break;
	default:
		goto no_match;
	}

	return true;

no_match:
	return false;
}

/*
 * Compatibility layer between the ustctl_basic_type struct and
 * _ustctl_basic_type union.
 */
static bool match_ustctl_field_basic_type(const struct ustctl_basic_type *first,
		const struct ustctl_basic_type *second)
{
	return match_ustctl_field_raw_basic_type(first->atype, &first->u.basic,
				second->atype, &second->u.basic);
}

int match_ustctl_field(const struct ustctl_field *first,
		const struct ustctl_field *second)
{
	/* Check the name of the field is identical. */
	if (strncmp(first->name, second->name, LTTNG_UST_ABI_SYM_NAME_LEN)) {
		goto no_match;
	}

	/* Check the field type is identical. */
	if (first->type.atype != second->type.atype) {
		goto no_match;
	}

	/* Check the field layout. */
	switch (first->type.atype) {
	case ustctl_atype_integer:
	case ustctl_atype_enum:
	case ustctl_atype_string:
	case ustctl_atype_float:
		if (!match_ustctl_field_raw_basic_type(first->type.atype,
					&first->type.u.legacy.basic, second->type.atype,
					&second->type.u.legacy.basic)) {
			goto no_match;
		}
		break;
	case ustctl_atype_sequence:
		/* Match element type of the sequence. */
		if (!match_ustctl_field_basic_type(&first->type.u.legacy.sequence.elem_type,
					&second->type.u.legacy.sequence.elem_type)) {
			goto no_match;
		}

		/* Match length type of the sequence. */
		if (!match_ustctl_field_basic_type(&first->type.u.legacy.sequence.length_type,
					&second->type.u.legacy.sequence.length_type)) {
			goto no_match;
		}
		break;
	case ustctl_atype_array:
		/* Match element type of the array. */
		if (!match_ustctl_field_basic_type(&first->type.u.legacy.array.elem_type,
					&second->type.u.legacy.array.elem_type)) {
			goto no_match;
		}

		/* Match length of the array. */
		if (first->type.u.legacy.array.length != second->type.u.legacy.array.length) {
			goto no_match;
		}
		break;
	case ustctl_atype_variant:
		/* Compare number of choice of the variants. */
		if (first->type.u.legacy.variant.nr_choices !=
					second->type.u.legacy.variant.nr_choices) {
			goto no_match;
		}

		/* Compare tag name of the variants. */
		if (strncmp(first->type.u.legacy.variant.tag_name,
					second->type.u.legacy.variant.tag_name,
					LTTNG_UST_ABI_SYM_NAME_LEN)) {
			goto no_match;
		}
		break;
	case ustctl_atype_struct:
		/* Compare number of fields of the structs. */
		if (first->type.u.legacy._struct.nr_fields != second->type.u.legacy._struct.nr_fields) {
			goto no_match;
		}
		break;
	case ustctl_atype_sequence_nestable:
		if (first->type.u.sequence_nestable.alignment != second->type.u.sequence_nestable.alignment) {
			goto no_match;
		}
		/* Compare length_name of the sequences. */
		if (strncmp(first->type.u.sequence_nestable.length_name,
					second->type.u.sequence_nestable.length_name,
					LTTNG_UST_ABI_SYM_NAME_LEN)) {
			goto no_match;
		}
		/* Comparison will be done when marshalling following items. */
		break;
	case ustctl_atype_array_nestable:
		if (first->type.u.array_nestable.alignment != second->type.u.array_nestable.alignment) {
			goto no_match;
		}
		/* Match length of the array. */
		if (first->type.u.array_nestable.length != second->type.u.array_nestable.length) {
			goto no_match;
		}
		/* Comparison of element type will be done when marshalling following item. */
		break;
	case ustctl_atype_enum_nestable:
		if (first->type.u.enum_nestable.id != second->type.u.enum_nestable.id) {
			goto no_match;
		}
		/* Compare name of the enums. */
		if (strncmp(first->type.u.enum_nestable.name,
					second->type.u.enum_nestable.name,
					LTTNG_UST_ABI_SYM_NAME_LEN)) {
			goto no_match;
		}
		/* Comparison of element type will be done when marshalling following item. */
		break;
	case ustctl_atype_struct_nestable:
		if (first->type.u.struct_nestable.alignment != second->type.u.struct_nestable.alignment) {
			goto no_match;
		}
		/* Compare number of fields of the structs. */
		if (first->type.u.struct_nestable.nr_fields != second->type.u.struct_nestable.nr_fields) {
			goto no_match;
		}
		break;
	case ustctl_atype_variant_nestable:
		if (first->type.u.variant_nestable.alignment != second->type.u.variant_nestable.alignment) {
			goto no_match;
		}
		/* Compare number of choice of the variants. */
		if (first->type.u.variant_nestable.nr_choices !=
					second->type.u.variant_nestable.nr_choices) {
			goto no_match;
		}

		/* Compare tag name of the variants. */
		if (strncmp(first->type.u.variant_nestable.tag_name,
					second->type.u.variant_nestable.tag_name,
					LTTNG_UST_ABI_SYM_NAME_LEN)) {
			goto no_match;
		}
		break;
	default:
		goto no_match;
	}

	return true;

no_match:
	return false;
}
