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

#ifndef __json_h__
#define __json_h__

#ifdef __cplusplus
extern "C" {
#endif

/* json Types: */
#define JSON_T_FALSE 0
#define JSON_T_TRUE 1
#define JSON_T_NULL 2
#define JSON_T_NUMBER 3
#define JSON_T_STRING 4
#define JSON_T_ARRAY 5
#define JSON_T_OBJECT 6

#define JSON_T_IS_REFERENCE 256

/* The json structure: */
struct json {
	struct json *next, *prev;	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct json *child;	/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;		/* The type of the item, as above. */

	char *valuestring;	/* The item's string, if type==json_String */
	int valueint;		/* The item's number, if type==json_Number */
	double valuedouble;	/* The item's number, if type==json_Number */

	char *string;		/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
};

struct json_hooks {
	void *(*malloc_fn) (size_t sz);
	void (*free_fn) (void *ptr);
};

/* Supply malloc, realloc and free functions to json */
extern void json_init_hooks(struct json_hooks *hooks);

/* Supply a block of JSON, and this returns a json object you can interrogate.
 * Call json_Delete when finished. */
extern struct json *json_parse(const char *value);

/* Supply a block of JSON, and this returns a json object you can interrogate.
 * Call json_Delete when finished.
 * end_ptr will point to 1 past the end of the JSON object */
extern struct json *json_parse_stream(const char *value, char **end_ptr);

/* Render a json entity to text for transfer/storage. Free the char* when finished. */
extern char *json_print(struct json *item);

/* Render a json entity to text for transfer/storage without any formatting.
 * Free the char* when finished. */
extern char *json_print_unformatted(struct json *item);

/* Delete a json entity and all subentities. */
extern void json_delete(struct json *c);

/* Returns the number of items in an array (or object). */
extern int json_get_array_size(struct json *array);

/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern struct json *json_get_array_item(struct json *array, int item);

/* Get item "string" from object. Case insensitive. */
extern struct json *json_get_object_item(struct json *object, const char *string);

/* These calls create a json item of the appropriate type. */
extern struct json *json_create_null(void);
extern struct json *json_create_true(void);
extern struct json *json_create_false(void);
extern struct json *json_create_bool(int b);
extern struct json *json_create_number(double num);
extern struct json *json_create_string(const char *string);
extern struct json *json_create_array(void);
extern struct json *json_create_object(void);

/* These utilities create an Array of count items. */
extern struct json *json_create_int_array(int *numbers, int count);
extern struct json *json_create_float_array(float *numbers, int count);
extern struct json *json_create_double_array(double *numbers, int count);
extern struct json *json_create_string_array(const char **strings, int count);

/* Append item to the specified array/object. */
extern void json_add_item_to_array(struct json *array, struct json *item);
extern void json_add_item_to_object(struct json *object,
				     const char *string, struct json *item);
/*
 * Append reference to item to the specified array/object. 
 * Use this when you want to add an existing json to a new json,
 * but don't want to corrupt your existing json.
 */
extern void json_add_item_reference_to_array(struct json *array,
					      struct json *item);
extern void json_add_item_reference_to_object(struct json *object,
					       const char *string, struct json *item);

/* Remove/Detatch items from Arrays/Objects. */
extern struct json *json_detach_item_from_array(struct json *array, int which);
extern void json_delete_item_from_array(struct json *array, int which);
extern struct json *json_detach_item_from_object(struct json *object,
					    const char *string);
extern void json_delete_item_from_object(struct json *object,
					  const char *string);

/* Update array items. */
extern void json_replace_item_in_array(struct json *array, int which,
				       struct json *newitem);
extern void json_replace_item_in_object(struct json *object,
					const char *string,
					struct json *newitem);

#define json_add_null_to_object(object,name)     json_add_item_to_object(object, name, json_create_null())
#define json_add_true_to_object(object,name)     json_add_item_to_object(object, name, json_create_true())
#define json_add_false_to_object(object,name)    json_add_item_to_object(object, name, json_create_false())
#define json_add_number_to_object(object,name,n) json_add_item_to_object(object, name, json_create_number(n))
#define json_add_string_to_object(object,name,s) json_add_item_to_object(object, name, json_create_string(s))

#ifdef __cplusplus
}
#endif
#endif
