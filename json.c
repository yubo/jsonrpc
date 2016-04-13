/*
  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/*
 * yubo@yubo.org
 * 2016-04-14
 */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "json.h"

static int json_strcasecmp(const char *s1, const char *s2)
{
	if (!s1)
		return (s1 == s2) ? 0 : 1;
	if (!s2)
		return 1;
	for (; tolower(*s1) == tolower(*s2); ++s1, ++s2)
		if (*s1 == 0)
			return 0;
	return tolower(*(const unsigned char *)s1) -
	    tolower(*(const unsigned char *)s2);
}

static void *(*json_malloc) (size_t sz) = malloc;
static void (*json_free) (void *ptr) = free;

static char *json_strdup(const char *str)
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if (!(copy = (char *)json_malloc(len)))
		return 0;
	memcpy(copy, str, len);
	return copy;
}

void json_init_hooks(struct json_hooks* hooks)
{
	if (!hooks) {		/* Reset hooks */
		json_malloc = malloc;
		json_free = free;
		return;
	}

	json_malloc = (hooks->malloc_fn) ? hooks->malloc_fn : malloc;
	json_free = (hooks->free_fn) ? hooks->free_fn : free;
}

/* Internal constructor. */
static struct json *json_new_item()
{
	struct json *node = (struct json *) json_malloc(sizeof(struct json));
	if (node)
		memset(node, 0, sizeof(*node));
	return node;
}

/* Delete a struct json structure. */
void json_delete(struct json *c)
{
	struct json *next;
	while (c) {
		next = c->next;
		if (!(c->type & JSON_T_IS_REFERENCE) && c->child)
			json_delete(c->child);
		if (!(c->type & JSON_T_IS_REFERENCE) && c->valuestring)
			json_free(c->valuestring);
		if (c->string)
			json_free(c->string);
		json_free(c);
		c = next;
	}
}

/* Parse the input text to generate a number, and populate the result into item. */
static char **parse_number(struct json *item, char **num)
{
	double n = 0, sign = 1, scale = 0;
	int subscale = 0, signsubscale = 1;

	if (**num == '-') {	/* Has sign? */
		sign = -1;
		(*num)++;
	}
	if (**num == '0')	/* is zero */
		(*num)++;
	if (**num >= '1' && **num <= '9') {	/* Number? */
		do {
			n = (n * 10.0) + (**num - '0');
			(*num)++;
		} while (**num >= '0' && **num <= '9');
	}
	if (**num == '.' && (*num)[1] >= '0' && (*num)[1] <= '9') {	/* Fractional part? */
		(*num)++;
		do {
			n = (n * 10.0) + (**num - '0');
			scale--;
			(*num)++;
		} while (**num >= '0' && **num <= '9');
	}
	if (**num == 'e' || **num == 'E') {	/* Exponent? */
		(*num)++;
		/* signed? */
		if (**num == '+')
			(*num)++;
		else if (**num == '-') {
			signsubscale = -1;
			(*num)++;
		}
		while (**num >= '0' && **num <= '9') {	/* Number? */
			subscale = (subscale * 10) + (**num - '0');
			(*num)++;
		}
	}

	n = sign * n * pow(10.0, (scale + subscale * signsubscale));	/* number = +/- number.fraction * 10^+/- exponent */

	item->valuedouble = n;
	item->valueint = (int)n;
	item->type = JSON_T_NUMBER;
	return num;
}

/* Render the number nicely from the given item into a string. */
static char *print_number(struct json *item)
{
	char *str;
	double d = item->valuedouble;
	if (fabs(((double)item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX
	    && d >= INT_MIN) {
		str = (char *)json_malloc(21);	/* 2^64+1 can be represented in 21 chars. */
		if (str)
			sprintf(str, "%d", item->valueint);
	} else {
		str = (char *)json_malloc(64);	/* This is a nice tradeoff. */
		if (str) {
			if (fabs(floor(d) - d) <= DBL_EPSILON)
				sprintf(str, "%.0f", d);
			else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
				sprintf(str, "%e", d);
			else
				sprintf(str, "%f", d);
		}
	}
	return str;
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const unsigned char firstByteMark[7] =
    { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static char **parse_string(struct json *item, char **str)
{
	char *ptr = *str + 1;
	char *ptr2;
	char *out;
	int len = 0;
	unsigned uc, uc2;
	if (**str != '\"')
		return NULL;	/* not a string! */

	while (*ptr != '\"' && *ptr && ++len)
		if (*ptr++ == '\\')
			ptr++;	/* Skip escaped quotes. */

	out = (char *)json_malloc(len + 1);	/* This is how long we need for the string, roughly. */
	if (!out)
		return 0;

	ptr = *str + 1;
	ptr2 = out;
	while (*ptr != '\"' && *ptr) {
		if (*ptr != '\\')
			*ptr2++ = *ptr++;
		else {
			ptr++;
			switch (*ptr) {
			case 'b':
				*ptr2++ = '\b';
				break;
			case 'f':
				*ptr2++ = '\f';
				break;
			case 'n':
				*ptr2++ = '\n';
				break;
			case 'r':
				*ptr2++ = '\r';
				break;
			case 't':
				*ptr2++ = '\t';
				break;
			case 'u':	/* transcode utf16 to utf8. */
				sscanf(ptr + 1, "%4x", &uc);
				ptr += 4;	/* get the unicode char. */

				if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0)
					break;	// check for invalid.

				if (uc >= 0xD800 && uc <= 0xDBFF)	// UTF16 surrogate pairs.
				{
					if (ptr[1] != '\\' || ptr[2] != 'u')
						break;	// missing second-half of surrogate.
					sscanf(ptr + 3, "%4x", &uc2);
					ptr += 6;
					if (uc2 < 0xDC00 || uc2 > 0xDFFF)
						break;	// invalid second-half of surrogate.
					uc = 0x10000 | ((uc & 0x3FF) << 10) |
					    (uc2 & 0x3FF);
				}

				len = 4;
				if (uc < 0x80)
					len = 1;
				else if (uc < 0x800)
					len = 2;
				else if (uc < 0x10000)
					len = 3;
				ptr2 += len;

				switch (len) {
				case 4:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 3:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 2:
					*--ptr2 = ((uc | 0x80) & 0xBF);
					uc >>= 6;
				case 1:
					*--ptr2 = (uc | firstByteMark[len]);
				}
				ptr2 += len;
				break;
			default:
				*ptr2++ = *ptr;
				break;
			}
			ptr++;
		}
	}
	*ptr2 = 0;
	if (*ptr == '\"')
		ptr++;
	item->valuestring = out;
	item->type = JSON_T_STRING;
	*str = ptr;
	return str;
}

/* Render the cstring provided to an escaped version that can be printed. */
static char *print_string_ptr(const char *str)
{
	const char *ptr;
	char *ptr2, *out;
	int len = 0;
	unsigned char token;

	if (!str)
		return json_strdup("");
	ptr = str;
	while ((token = *ptr) && ++len) {
		if (strchr("\"\\\b\f\n\r\t", token))
			len++;
		else if (token < 32)
			len += 5;
		ptr++;
	}

	out = (char *)json_malloc(len + 3);
	if (!out)
		return 0;

	ptr2 = out;
	ptr = str;
	*ptr2++ = '\"';
	while (*ptr) {
		if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\')
			*ptr2++ = *ptr++;
		else {
			*ptr2++ = '\\';
			switch (token = *ptr++) {
			case '\\':
				*ptr2++ = '\\';
				break;
			case '\"':
				*ptr2++ = '\"';
				break;
			case '\b':
				*ptr2++ = 'b';
				break;
			case '\f':
				*ptr2++ = 'f';
				break;
			case '\n':
				*ptr2++ = 'n';
				break;
			case '\r':
				*ptr2++ = 'r';
				break;
			case '\t':
				*ptr2++ = 't';
				break;
			default:
				sprintf(ptr2, "u%04x", token);
				ptr2 += 5;
				break;	/* escape and print */
			}
		}
	}
	*ptr2++ = '\"';
	*ptr2++ = 0;
	return out;
}

/* Invote print_string_ptr (which is useful) on an item. */
static char *print_string(struct json *item)
{
	return print_string_ptr(item->valuestring);
}

/* Predeclare these prototypes. */
static char **parse_value(struct json *item, char **value);
static char *print_value(struct json *item, int depth, int fmt);
static char **parse_array(struct json *item, char **value);
static char *print_array(struct json *item, int depth, int fmt);
static char **parse_object(struct json *item, char **value);
static char *print_object(struct json *item, int depth, int fmt);

/* Utility to jump whitespace and cr/lf */
static inline char **skip(char **in)
{
	if (in && *in)
		while (isspace(**in))
			(*in)++;
	return in;
}

/* Parse an object - create a new root, and populate. */
struct json *json_parse(const char *value)
{
	struct json *c = json_new_item();
	if (!c)
		return 0;	/* memory fail */

	char **end_ptr = (char **)&value;

	if (!parse_value(c, skip(end_ptr))) {
		json_delete(c);
		return 0;
	}
	return c;
}

/* Parse an object - create a new root, and populate
 *  Also indicates where in the stream the Object ends. */
struct json *json_parse_stream(const char *value, char **end_ptr)
{
	if (!end_ptr)
		return NULL;
	struct json *c = json_new_item();
	if (!c)
		return 0;	/* memory fail */

	*end_ptr = (char *)value;

	if (!parse_value(c, skip(end_ptr))) {
		json_delete(c);
		return 0;
	}
	return c;
}

/* Render a struct json item/entity/structure to text. */
char *json_print(struct json *item)
{
	return print_value(item, 0, 1);
}

char *json_print_unformatted(struct json *item)
{
	return print_value(item, 0, 0);
}

static int stream_cmp(char **stream, const char *str)
{
	while (**stream == *str) {
		(*stream)++;
		str++;
	}
	if (*str == '\0')
		return 0;
	return **stream - *str;
}

/* Parser core - when encountering text, process appropriately. */
static char **parse_value(struct json *item, char **value)
{
	if (!stream_cmp(value, "null")) {
		item->type = JSON_T_NULL;
		return value;
	} else if (!stream_cmp(value, "false")) {
		item->type = JSON_T_FALSE;
		return value;
	} else if (!stream_cmp(value, "true")) {
		item->type = JSON_T_TRUE;
		item->valueint = 1;
		return value;
	}

	switch (**value) {
	case '"':
		return parse_string(item, value);
	case '-':
	case '0' ... '9':
		return parse_number(item, value);
	case '[':
		return parse_array(item, value);
	case '{':
		return parse_object(item, value);
	}

	return NULL;		/* failure */
}

/* Render a value to text. */
static char *print_value(struct json *item, int depth, int fmt)
{
	char *out = 0;
	if (!item)
		return 0;
	switch ((item->type) & 255) {
	case JSON_T_NULL:
		out = json_strdup("null");
		break;
	case JSON_T_FALSE:
		out = json_strdup("false");
		break;
	case JSON_T_TRUE:
		out = json_strdup("true");
		break;
	case JSON_T_NUMBER:
		out = print_number(item);
		break;
	case JSON_T_STRING:
		out = print_string(item);
		break;
	case JSON_T_ARRAY:
		out = print_array(item, depth, fmt);
		break;
	case JSON_T_OBJECT:
		out = print_object(item, depth, fmt);
		break;
	}
	return out;
}

/* Build an array from input text. */
static char **parse_array(struct json *item, char **value)
{
	struct json *child;
	if (**value != '[')	/* not an array! */
		return NULL;

	item->type = JSON_T_ARRAY;
	(*value)++;
	skip(value);
	if (**value == ']') {
		(*value)++;
		return value;	/* empty array. */
	}

	item->child = child = json_new_item();
	if (!item->child)
		return 0;	/* memory fail */
	if (!skip(parse_value(child, value)))	/* skip any spacing, get the value. */
		return NULL;

	while (**value == ',') {
		(*value)++;
		skip(value);
		if (**value == ']') {
			(*value)++;
			break;
		}

		struct json *new_item;
		if (!(new_item = json_new_item()))
			return 0;	/* memory fail */
		child->next = new_item;
		new_item->prev = child;
		child = new_item;
		if (!skip(parse_value(child, value)))
			return 0;	/* memory fail */
	}

	if (**value != ']')
		return NULL;
	(*value)++;

	return value;
}

/* Render an array to text */
static char *print_array(struct json *item, int depth, int fmt)
{
	char **entries;
	char *out = 0, *ptr, *ret;
	int len = 5;
	struct json *child = item->child;
	int numentries = 0, i = 0, fail = 0;

	/* How many entries in the array? */
	while (child)
		numentries++, child = child->next;
	/* Allocate an array to hold the values for each */
	entries = (char **)json_malloc(numentries * sizeof(char *));
	if (!entries)
		return 0;
	memset(entries, 0, numentries * sizeof(char *));
	/* Retrieve all the results: */
	child = item->child;
	while (child && !fail) {
		ret = print_value(child, depth + 1, fmt);
		entries[i++] = ret;
		if (ret)
			len += strlen(ret) + 2 + (fmt ? 1 : 0);
		else
			fail = 1;
		child = child->next;
	}

	/* If we didn't fail, try to malloc the output string */
	if (!fail)
		out = (char *)json_malloc(len);
	/* If that fails, we fail. */
	if (!out)
		fail = 1;

	/* Handle failure. */
	if (fail) {
		for (i = 0; i < numentries; i++)
			if (entries[i])
				json_free(entries[i]);
		json_free(entries);
		return 0;
	}

	/* Compose the output array. */
	*out = '[';
	ptr = out + 1;
	*ptr = 0;
	for (i = 0; i < numentries; i++) {
		strcpy(ptr, entries[i]);
		ptr += strlen(entries[i]);
		if (i != numentries - 1) {
			*ptr++ = ',';
			if (fmt)
				*ptr++ = ' ';
			*ptr = 0;
		}
		json_free(entries[i]);
	}
	json_free(entries);
	*ptr++ = ']';
	*ptr++ = 0;
	return out;
}

/* Build an object from the text. */
static char **parse_object(struct json *item, char **value)
{
	struct json *child;
	if (**value != '{')
		return NULL;	/* not an object! */

	item->type = JSON_T_OBJECT;
	(*value)++;
	skip(value);
	if (**value == '}') {
		(*value)++;
		return value;	/* empty object. */
	}

	item->child = child = json_new_item();
	if (!item->child)
		return 0;
	if (!skip(parse_string(child, value)))
		return 0;
	child->string = child->valuestring;
	child->valuestring = 0;
	if (**value != ':')
		return NULL;	/* fail! */
	(*value)++;
	if (!skip(parse_value(child, skip(value))))	/* skip any spacing, get the value. */
		return 0;

	while (**value == ',') {
		(*value)++;
		skip(value);

		if (**value == '}') {
			(*value)++;
			break;
		}

		struct json *new_item;
		if (!(new_item = json_new_item()))
			return 0;	/* memory fail */
		child->next = new_item;
		new_item->prev = child;
		child = new_item;
		if (!skip(parse_string(child, value)))
			return 0;
		child->string = child->valuestring;
		child->valuestring = 0;
		if (**value != ':')
			return NULL;	/* fail! */
		(*value)++;
		if (!skip(parse_value(child, skip(value))))	/* skip any spacing, get the value. */
			return 0;
	}

	if (**value != '}')
		return NULL;
	(*value)++;

	return value;
}

/* Render an object to text. */
static char *print_object(struct json *item, int depth, int fmt)
{
	char **entries = 0, **names = 0;
	char *out = 0, *ptr, *ret, *str;
	int len = 7, i = 0, j;
	struct json *child = item->child;
	int numentries = 0, fail = 0;
	/* Count the number of entries. */
	while (child)
		numentries++, child = child->next;
	/* Allocate space for the names and the objects */
	entries = (char **)json_malloc(numentries * sizeof(char *));
	if (!entries)
		return 0;
	names = (char **)json_malloc(numentries * sizeof(char *));
	if (!names) {
		json_free(entries);
		return 0;
	}
	memset(entries, 0, sizeof(char *) * numentries);
	memset(names, 0, sizeof(char *) * numentries);

	/* Collect all the results into our arrays: */
	child = item->child;
	depth++;
	if (fmt)
		len += depth;
	while (child) {
		names[i] = str = print_string_ptr(child->string);
		entries[i++] = ret = print_value(child, depth, fmt);
		if (str && ret)
			len +=
			    strlen(ret) + strlen(str) + 2 + (fmt ? 2 +
							     depth : 0);
		else
			fail = 1;
		child = child->next;
	}

	/* Try to allocate the output string */
	if (!fail)
		out = (char *)json_malloc(len);
	if (!out)
		fail = 1;

	/* Handle failure */
	if (fail) {
		for (i = 0; i < numentries; i++) {
			if (names[i])
				json_free(names[i]);
			if (entries[i])
				json_free(entries[i]);
		}
		json_free(names);
		json_free(entries);
		return 0;
	}

	/* Compose the output: */
	*out = '{';
	ptr = out + 1;
	if (fmt)
		*ptr++ = '\n';
	*ptr = 0;
	for (i = 0; i < numentries; i++) {
		if (fmt)
			for (j = 0; j < depth; j++)
				*ptr++ = '\t';
		strcpy(ptr, names[i]);
		ptr += strlen(names[i]);
		*ptr++ = ':';
		if (fmt)
			*ptr++ = '\t';
		strcpy(ptr, entries[i]);
		ptr += strlen(entries[i]);
		if (i != numentries - 1)
			*ptr++ = ',';
		if (fmt)
			*ptr++ = '\n';
		*ptr = 0;
		json_free(names[i]);
		json_free(entries[i]);
	}

	json_free(names);
	json_free(entries);
	if (fmt)
		for (i = 0; i < depth - 1; i++)
			*ptr++ = '\t';
	*ptr++ = '}';
	*ptr++ = 0;
	return out;
}

/* Get Array size/item / object item. */
int json_get_array_size(struct json *array)
{
	struct json *c = array->child;
	int i = 0;
	while (c)
		i++, c = c->next;
	return i;
}

struct json *json_get_array_item(struct json *array, int item)
{
	struct json *c = array->child;
	while (c && item > 0)
		item--, c = c->next;
	return c;
}

struct json *json_get_object_item(struct json *object, const char *string)
{
	struct json *c = object->child;
	while (c && json_strcasecmp(c->string, string))
		c = c->next;
	return c;
}

/* Utility for array list handling. */
static void suffix_object(struct json *prev, struct json *item)
{
	prev->next = item;
	item->prev = prev;
}

/* Utility for handling references. */
static struct json *create_reference(struct json *item)
{
	struct json *ref = json_new_item();
	if (!ref)
		return 0;
	memcpy(ref, item, sizeof(*ref));
	ref->string = 0;
	ref->type |= JSON_T_IS_REFERENCE;
	ref->next = ref->prev = 0;
	return ref;
}

/* Add item to array/object. */
void json_add_item_to_array(struct json *array, struct json *item)
{
	struct json *c = array->child;
	if (!item)
		return;
	if (!c) {
		array->child = item;
	} else {
		while (c && c->next)
			c = c->next;
		suffix_object(c, item);
	}
}

void json_add_item_to_object(struct json *object, const char *string,
			   struct json *item)
{
	if (!item)
		return;
	if (item->string)
		json_free(item->string);
	item->string = json_strdup(string);
	json_add_item_to_array(object, item);
}

void json_add_item_reference_to_array(struct json *array, struct json *item)
{
	json_add_item_to_array(array, create_reference(item));
}

void json_add_item_reference_to_object(struct json *object, const char *string,
				    struct json *item)
{
	json_add_item_to_object(object, string, create_reference(item));
}

struct json *json_detach_item_from_array(struct json *array, int which)
{
	struct json *c = array->child;
	while (c && which > 0)
		c = c->next, which--;
	if (!c)
		return 0;
	if (c->prev)
		c->prev->next = c->next;
	if (c->next)
		c->next->prev = c->prev;
	if (c == array->child)
		array->child = c->next;
	c->prev = c->next = 0;
	return c;
}

void json_delete_item_from_array(struct json *array, int which)
{
	json_delete(json_detach_item_from_array(array, which));
}

struct json *json_detach_item_from_object(struct json *object,
					 const char *string)
{
	int i = 0;
	struct json *c = object->child;
	while (c && json_strcasecmp(c->string, string))
		i++, c = c->next;
	if (c)
		return json_detach_item_from_array(object, i);
	return 0;
}

void json_delete_item_from_object(struct json *object, const char *string)
{
	json_delete(json_detach_item_from_object(object, string));
}

/* Replace array/object items with new ones. */
void json_replace_item_in_array(struct json *array, int which,
			      struct json *newitem)
{
	struct json *c = array->child;
	while (c && which > 0)
		c = c->next, which--;
	if (!c)
		return;
	newitem->next = c->next;
	newitem->prev = c->prev;
	if (newitem->next)
		newitem->next->prev = newitem;
	if (c == array->child)
		array->child = newitem;
	else
		newitem->prev->next = newitem;
	c->next = c->prev = 0;
	json_delete(c);
}

void json_replace_item_in_object(struct json *object, const char *string,
			       struct json *newitem)
{
	int i = 0;
	struct json *c = object->child;
	while (c && json_strcasecmp(c->string, string))
		i++, c = c->next;
	if (c) {
		newitem->string = json_strdup(string);
		json_replace_item_in_array(object, i, newitem);
	}
}

/* Create basic types: */
struct json *json_create_null()
{
	struct json *item = json_new_item();
	if (item)
		item->type = JSON_T_NULL;
	return item;
}

struct json *json_create_true()
{
	struct json *item = json_new_item();
	if (item)
		item->type = JSON_T_TRUE;
	return item;
}

struct json *json_create_false()
{
	struct json *item = json_new_item();
	if (item)
		item->type = JSON_T_FALSE;
	return item;
}

struct json *json_create_bool(int b)
{
	struct json *item = json_new_item();
	if (item)
		item->type = b ? JSON_T_TRUE : JSON_T_FALSE;
	return item;
}

struct json *json_create_number(double num)
{
	struct json *item = json_new_item();
	if (item) {
		item->type = JSON_T_NUMBER;
		item->valuedouble = num;
		item->valueint = (int)num;
	}
	return item;
}

struct json *json_create_string(const char *string)
{
	struct json *item = json_new_item();
	if (item) {
		item->type = JSON_T_STRING;
		item->valuestring = json_strdup(string);
	}
	return item;
}

struct json *json_create_array()
{
	struct json *item = json_new_item();
	if (item)
		item->type = JSON_T_ARRAY;
	return item;
}

struct json *json_create_object()
{
	struct json *item = json_new_item();
	if (item)
		item->type = JSON_T_OBJECT;
	return item;
}

/* Create Arrays: */
struct json *json_create_int_array(int *numbers, int count)
{
	int i;
	struct json *n = 0, *p = 0, *a = json_create_array();
	for (i = 0; a && i < count; i++) {
		n = json_create_number(numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	}
	return a;
}

struct json *json_create_float_array(float *numbers, int count)
{
	int i;
	struct json *n = 0, *p = 0, *a = json_create_array();
	for (i = 0; a && i < count; i++) {
		n = json_create_number(numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	}
	return a;
}

struct json *json_create_double_array(double *numbers, int count)
{
	int i;
	struct json *n = 0, *p = 0, *a = json_create_array();
	for (i = 0; a && i < count; i++) {
		n = json_create_number(numbers[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	}
	return a;
}

struct json *json_create_string_array(const char **strings, int count)
{
	int i;
	struct json *n = 0, *p = 0, *a = json_create_array();
	for (i = 0; a && i < count; i++) {
		n = json_create_string(strings[i]);
		if (!i)
			a->child = n;
		else
			suffix_object(p, n);
		p = n;
	}
	return a;
}
