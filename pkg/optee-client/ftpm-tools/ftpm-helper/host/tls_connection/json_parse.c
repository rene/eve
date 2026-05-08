#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <jansson.h>
#include "json_parse.h"
#include <assert.h>

struct msg_obj_heap {
	uint8_t *	current;
	uint8_t *	endptr;
};

struct msg_obj_metadata {
	int		refcount;
	uint8_t *	correlator;
	int		correlator_sz;
	char *		response_topic;
};

#define BOUNDS_CHECK(idx, endptr, parser) do { if ((intptr_t)idx > (intptr_t)endptr) { fprintf(stderr, "ERR: BOUNDS VIOLATION for %s %p > %p\n", parser->json_name, idx, endptr); } assert(((intptr_t)idx <= (intptr_t)endptr)); } while (0)

#ifdef CFG_SEQ_FTPM_TEST
#define DBGPRNT printf
#else
#define DBGPRNT(...)
#endif

void msg_obj_hold(msg_obj cdata)
{

	struct msg_obj_metadata *data = (struct msg_obj_metadata *)(((uint8_t *)cdata) - sizeof(struct msg_obj_metadata));
	__sync_add_and_fetch(&data->refcount, 1);

}
void msg_obj_put(void *cdata)
{
	int newref;
	struct msg_obj_metadata *data = (struct msg_obj_metadata *)(((uint8_t *)cdata) - sizeof(struct msg_obj_metadata));

	newref = __sync_sub_and_fetch(&data->refcount, 1);
	DBGPRNT("DEBUG: MSG_PUT: new ref count for object at %p (refcount ptr %p) is %u\n", cdata, &data->refcount, newref);
	if (newref < 0) {
		fprintf(stderr, "ERR: MSG REFCOUNT UNDERFLOW!  ABORTING!\n");
		abort();
	}
	if (!newref) {
		/*
		 * Note: we have to free data, here, not cdata, as data points
		 * to the start of the full message block
		 */
		if (data->correlator) {
			free(data->correlator);
		}
		if (data->response_topic) {
			free(data->response_topic);
		}
		DBGPRNT("DEBUG: Freeing message %p\n", data);
		free(data);
	}
}

static msg_obj msg_obj_alloc(size_t size)
{
	void *dbuf;

	size += sizeof(struct msg_obj_metadata);

	dbuf = malloc(size);
	if (!dbuf) {
		return NULL;
	}
	memset(dbuf, 0, size);
	dbuf += sizeof(struct msg_obj_metadata);
	msg_obj_hold(dbuf);
	return dbuf;
}


void *dbuf_alloc(struct msg_obj_heap *heapptr, size_t objsize, struct msg_parser *parser, char *indent)
{
	void *ptr = heapptr->current;

	BOUNDS_CHECK(heapptr->current, heapptr->endptr, parser);
	heapptr->current += objsize;
	DBGPRNT("DEBUG: %sdbuf alloc new current:endptr is %p:%p\n", indent, heapptr->current, heapptr->endptr);
	return ptr;
}

static size_t compute_dynamic_dbuf_size(struct msg_parser *parser, json_t *msgobj)
{
	size_t i;
	json_t *value;
	size_t objsize = 0;
	const char *key;

	switch (parser->obj_type) {
	case JSON_ARRAY:
		json_array_foreach(msgobj, i, value) {
			objsize += compute_dynamic_dbuf_size(&parser->children[0], value);
		}
		if (parser->dynamic) {
			/* if we are a dynamic array, we need to add the
			 * size of the array element here, times the
			 * number of elements in the array
			 * it wont be included in the top level static
			 * size
			 */
			objsize += parser->children[0].alignsize * json_array_size(msgobj);
		}
		DBGPRNT("DEBUG: ARRAY ELEMENTS for %s has dynamic size %zu\n", parser->json_name, objsize);
		break;

	case JSON_OBJECT:
		json_object_foreach(msgobj, key, value) {
			for (i = 0; i < parser->num_children; i++) {
				/* find the corresponding parser */
				if (!strcmp(key, parser->children[i].json_name)) {
					objsize += compute_dynamic_dbuf_size(&parser->children[i], value);
				}
			}
			DBGPRNT("DEBUG: OBJECT %s has dynamic size %zu\n", parser->json_name, objsize);
		}
		break;

	case JSON_INTEGER:
		break;

	case JSON_STRING:
		if (parser->dynamic) {
			objsize += json_string_length(msgobj) + 1;
		}
		break;

	case JSON_TRUE:
	case JSON_FALSE:
		break;

	default:
		fprintf(stderr, "ERR: Cannot compute size for %s\n", parser->json_name);
		break;
	}

	return objsize;
}
/**
 * @fn size_t compute_dbug_size(struct msg_parser *parser)
 * @brief computes the size we need for a json to c conversion
 * This is a bit of a wierd function.  The size of a buffer is broken into two
 * parts: the static size, and the heap.  The parser that we are passed should
 * contain the static size of the buffer we need.  The heap is computed by doing
 * a traversal of the parse tree, and accumulating the size of every dynamic
 * object
 * @param parser - the parse tree to compute
 * @param msgobj - the msgobj to traverse
 */
static size_t compute_dbuf_size(struct msg_parser *parser, json_t *msgobj)
{
	size_t objsize = 0;

	objsize = compute_dynamic_dbuf_size(parser, msgobj);
	DBGPRNT("DEBUG: OBJECT %s has static size %zu\n", parser->json_name, parser->alignsize);
	return objsize + parser->alignsize;
}


static int create_struct_from_json(uint8_t *dbuf, json_t *msgobj, struct msg_parser *parser, struct msg_parser *parent, const uint8_t *endptr, struct msg_obj_heap *heapptr, char *indent)
{
	uint8_t *idx = NULL;
	const char *key;
	json_t *value;
	int rc = -EINVAL;
	size_t i;
	size_t arrayidx;
	size_t objsize;
	void **arryptr;
	void **sptr;
	size_t *arraysz;
	bool found;
	int *intval;
	uint64_t *int64val;
	char *sval;
	bool *bval;
	int ival;
	uint64_t i64val;

#ifdef PARSER_DEBUG
	char *newindent = alloca(strlen(indent) + 2);
	strcpy(newindent, " ");
	newindent = strcat(newindent, indent);
#else
	char *newindent = indent;
#endif
	if (json_typeof(msgobj) != (json_type)parser->obj_type) {
		if ((parser->obj_type != JSON_TRUE) || (!(json_is_true(msgobj) || json_is_false(msgobj)))) {
			fprintf(stderr, "ERR: parser/json type mismatch: %d %d\n", json_typeof(msgobj), parser->obj_type);
		}
	}
	
	DBGPRNT("DEBUG: %sDecoding field %s\n", indent, parser->json_name ? parser->json_name : "ANON");
	switch (json_typeof(msgobj)) {
	case JSON_OBJECT:
		/* arrays get their offset set by the array parser */
		if (parent && parent->obj_type == JSON_ARRAY) {
			idx = dbuf;
			DBGPRNT("DEBUG: %sOBJECT: Starting at %p for %s\n", indent, idx, parser->json_name);
		} else {
			idx = dbuf + parser->offset;
			DBGPRNT("DEBUG: %sOBJECT: Starting at %p (offset %zu) for %s\n", indent, idx, parser->offset, parser->json_name);
		}
		BOUNDS_CHECK(idx, endptr, parser);
		json_object_foreach(msgobj, key, value) {
			found = false;
			for (i = 0; i < parser->num_children; i++) {
				/* find the corresponding parser */
				if (!strcmp(key, parser->children[i].json_name)) {
					found = true;
					rc = create_struct_from_json(idx, value, &parser->children[i], parser, endptr, heapptr, newindent);
					if (rc) {
						goto out;
					}
				}
			}
			if (found == false) {
				fprintf(stderr, "ERR: No child parser found for %s, ignoring\n", key);
				//rc = -ENOENT;
				rc = 0;
				goto out;
			}
		}
		break;

	case JSON_ARRAY:
		if (parser->dynamic) {
			objsize = json_array_size(msgobj) * parser->children[0].realsize;
			DBGPRNT("DEBUG: %sARRAY: allocating %zu bytes for array %s\n", indent, json_array_size(msgobj) * parser->children[0].realsize, parser->json_name);
			idx = dbuf_alloc(heapptr, objsize, parser, indent);
			arryptr = (void **)(dbuf + parser->offset);
			*arryptr = idx;
			arraysz = (size_t *)(dbuf + parser->acoffset);
			*arraysz = json_array_size(msgobj);
			DBGPRNT("DEBUG: %sARRAY: starting at heap area %p for array %s with element count %zu\n", indent, idx, parser->json_name, json_array_size(msgobj));
		} else {
			idx = dbuf + parser->offset;
			DBGPRNT("DEBUG: %sARRAY: starting at %p (offset %zu) for array %s\n", indent, idx, parser->offset, parser->json_name);
		}
		json_array_foreach(msgobj, arrayidx, value) {
			if (!parser->dynamic && arrayidx >= parser->realsize) {
				fprintf(stderr, "ERR: Array exceeds maximum size\n");
				rc = -ERANGE;
				goto out;
			}
			if (arrayidx > 0) {
				idx = idx + parser->children[0].offset;
			}
			DBGPRNT("DEBUG: %sARRAY: Advancing buffer by %zu bytes to %p for array index %zu\n", indent, arrayidx == 0 ? 0 : parser->children[0].offset, idx, arrayidx);
			BOUNDS_CHECK(idx, endptr, parser);
			rc = create_struct_from_json(idx, value, &parser->children[0], parser, endptr, heapptr, newindent);
			if (rc) {
				goto out;
			}
		}
		break;

	case JSON_INTEGER:
		/* array elements do our indexing for us */
		if (parent && parent->obj_type == JSON_ARRAY) {
			idx = dbuf;
			DBGPRNT("DEBUG: %sINTEGER: Starting at %p for %s\n", indent, idx, parser->json_name);
		} else {
			idx = (uint8_t *)(dbuf + parser->offset);
		}
		BOUNDS_CHECK(idx, endptr, parser);
		if (parser->realsize == sizeof(int)) {
			intval = (int *)(idx);
			ival = (int)json_integer_value(msgobj);
			memcpy(intval, &ival, sizeof(ival));
			DBGPRNT("DEBUG: %sINTEGER: writing int length %zu value %u to %p (offset %zu)\n", indent, sizeof(ival), ival, idx, parser->offset);
		} else if (parser->realsize == sizeof(uint64_t)) {
			int64val = (uint64_t *)(idx);
			i64val = (uint64_t)json_integer_value(msgobj);
			memcpy(int64val, &i64val, sizeof(i64val));
			DBGPRNT("DEBUG: %sINTEGER: writing int length %zu value %"PRIu64" to %p (offset %zu)\n", indent, sizeof(i64val), i64val, idx, parser->offset);
		}
		break;

	case JSON_STRING:
		if (parser->anon) {
			sptr = (void **)dbuf;
			DBGPRNT("DEBUG: %sSTRING: allocating %zu bytes from heap for string %s\n", indent, json_string_length(msgobj) + 1, parser->json_name ? parser->json_name : "Anon");
			idx = dbuf_alloc(heapptr, json_string_length(msgobj) + 1, parser, indent);
			BOUNDS_CHECK(idx, endptr, parser);
			*sptr = idx;
			sval = (char *)idx;
			memset(sval, 0, json_string_length(msgobj) + 1);
			strncpy(sval, json_string_value(msgobj), json_string_length(msgobj));
		} else if (parser->dynamic) {
			sptr = (void **)(dbuf + parser->offset);
			DBGPRNT("DEBUG: %sSTRING: allocating %zu bytes from heap for string %s\n", indent, json_string_length(msgobj) + 1, parser->json_name);
			idx = dbuf_alloc(heapptr, json_string_length(msgobj) + 1, parser, indent);
			BOUNDS_CHECK(idx, endptr, parser);
			*sptr = idx;
			sval = (char *)idx;
			memset(sval, 0, json_string_length(msgobj) + 1);
			strncpy(sval, json_string_value(msgobj), json_string_length(msgobj));
		} else {
			idx = (uint8_t *)(dbuf + parser->offset);
			BOUNDS_CHECK(idx, endptr, parser);
			sval = (char *)idx;
			memset(sval, 0, parser->realsize);
			strncpy(sval, json_string_value(msgobj), parser->realsize);
		}
		DBGPRNT("DEBUG: %sSTRING writing string length %zu value %s to %p (offset %zu)\n", indent, strlen(json_string_value(msgobj)), json_string_value(msgobj), idx, parser->offset);
		break;

	case JSON_TRUE:
		idx = (uint8_t *)(dbuf + parser->offset);
		DBGPRNT("DEBUG: %sTRUE: starting at %p (offset %zu) for %s\n", indent, idx, parser->offset, parser->json_name);
		BOUNDS_CHECK(idx, endptr, parser);
		bval = (bool *)idx;
		*bval = true;
		break;

	case JSON_FALSE:
		idx = (uint8_t *)(dbuf + parser->offset);
		DBGPRNT("DEBUG: %sFALSE: starting at %p (offset %zu) for %s\n", indent, idx, parser->offset, parser->json_name);
		BOUNDS_CHECK(idx, endptr, parser);
		bval = (bool *)idx;
		*bval = false;
		break;

	case JSON_NULL:
		sptr = (void **)(dbuf + parser->offset);
		DBGPRNT("DEBUG: %sAssigning NULL to pointer at address %p\n", indent, idx);
		BOUNDS_CHECK(idx, endptr, parser);
		*sptr = NULL;
		break;

	default:
		fprintf(stderr, "ERR: Unknown type converting object to json\n");
		break;
	}


	rc = 0;
out:
	return rc;
}

msg_obj json_to_c(char *inbuf, size_t insz, struct msg_parser *parser)
{
	uint8_t *dbuf = NULL;
	char *einbuf;
	uint8_t *endptr;
	json_t *msgobj;
	json_error_t err;
	int rc;
	size_t objsize;
	struct msg_obj_heap heapdata;

	if ((parser == NULL) || (inbuf == NULL)) {
		fprintf(stderr, "ERR: Unable to convert message to struct\n");
		return NULL;
	}

	einbuf = alloca(insz + 1);
	if (!einbuf) {
		fprintf(stderr, "ERR: Unable to allocate extended translation buffer\n");
	}
	memcpy(einbuf, inbuf, insz);
	einbuf[insz] = '\0';

	DBGPRNT("DEBUG: JSON 2 C: %s\n", einbuf);

	msgobj = json_loadb(einbuf, insz, JSON_ALLOW_NUL, &err);
	if (!msgobj) {
		fprintf(stderr, "ERR: Unable to parse json file at line %d, col %d: %s\n", err.line, err.column, err.text);
		goto out;
	}

	objsize = compute_dbuf_size(parser, msgobj);

	DBGPRNT("DEBUG: allocating %zu bytes to hold converted json message\n", objsize);
	dbuf = msg_obj_alloc(objsize);
	if (!dbuf) {
		fprintf(stderr, "ERR: Unable to allocate memory for data buffer\n");
		goto out;
	}

	endptr = dbuf + objsize;
	heapdata.current = dbuf;
	heapdata.endptr = endptr;
	/* reserve top level struct data */
	DBGPRNT("DEBUG: Reserving %zu bytes in heap data for top level struct\n", parser->alignsize);
	DBGPRNT("DEBUG: dbuf is %p\n", dbuf);
	DBGPRNT("DEBUG: endptr is %p\n", endptr);
	dbuf_alloc(&heapdata, parser->alignsize, parser, " ");

	rc = create_struct_from_json(dbuf, msgobj, parser, NULL, endptr, &heapdata, " ");
	json_decref(msgobj);
	if (rc) {
		fprintf(stderr, "ERR: Unable to create data structure from json tree\n");
		goto out_free;
	}

out:
	return dbuf;
out_free:
	msg_obj_put(dbuf);
	dbuf = NULL;
	goto out;
}


/**
 * @file
 * @brief this api defines the registration methods we use for registering
 * paths to publish on and subscribe to for our comms channel to the cloud
 */
static json_t *create_json_object(uint8_t *data, struct msg_parser *parser, json_t *parent, char *indent)
{
	json_t *newelem = NULL;
	json_t *retelem = NULL;
	uint8_t *idx;
	int *intval;
	uint64_t *int64val;
	char *sval;
	bool *bval;
	size_t i;
	size_t arraysize;

#ifdef PARSER_DEBUG
	char *newindent = alloca(strlen(indent) + 2);
	strcpy(newindent, " ");
	newindent = strcat(newindent, indent);
	char *tmpstr;
#else
	char *newindent = indent;
#endif
	switch (parser->obj_type) {
	case JSON_OBJECT:
		newelem = json_object();
		/* array elements do our indexing for us */
		if (parent && json_typeof(parent) == JSON_ARRAY) {
			idx = data;
		} else {
			idx = data + parser->offset;
		}
		if (parser->num_children) {
			for (i = 0; i < parser->num_children; i++) {
				retelem = create_json_object(idx, &parser->children[i], newelem, newindent);
				json_object_set(newelem, parser->children[i].json_name, retelem);
				json_decref(retelem);
			}
		}
		break;

	case JSON_ARRAY:
		newelem = json_array();
		if (parser->dynamic) {
			arraysize = *(size_t *)(data + parser->acoffset);
			for (i = 0; i < arraysize; i++) {
				idx = *(void **)(data + parser->offset);
				idx = idx + (parser->children[0].offset * i);
				retelem = create_json_object(idx, &parser->children[0], newelem, newindent);
				json_array_append_new(newelem, retelem);
			}
		} else {
			for (i = 0; i < parser->realsize; i++) {
				idx = data + parser->offset + (parser->children[0].offset * i);
				retelem = create_json_object(idx, &parser->children[0], newelem, newindent);
				/* json_array_set_new steals the ref */
				json_array_append_new(newelem, retelem);
			}
		}
		break;

	case JSON_INTEGER:
		/* array elements do our indexing for us */
		if (parent && json_typeof(parent) == JSON_ARRAY) {
			idx = data;
		} else {
			idx = (uint8_t *)(data + parser->offset);
		}
		if (parser->realsize == sizeof(int)) {
			intval = (int *)(idx);
			newelem = json_integer(*intval);
		} else if (parser->realsize == sizeof(uint64_t)) {
			int64val = (uint64_t *)(idx);
			newelem = json_integer(*int64val);
		}
		break;

	case JSON_STRING:
		if (parser->anon) {
			idx = (*((uint8_t **)data));
		} else {
			if (parser->dynamic) {
				idx = (uint8_t *)(*(char **)(data + parser->offset));
			} else {
				idx = (uint8_t *)(data + parser->offset);
			}
		}

		sval = (char *)idx;
		newelem = json_string(sval);
		break;

	case JSON_TRUE:
	case JSON_FALSE:
		/* bool values get handled the same */
		idx = (uint8_t *)(data + parser->offset);
		bval = (bool *)idx;
		if (*bval) {
			newelem = json_true();
		} else {
			newelem = json_false();
		}
		break;

	default:
		fprintf(stderr, "ERR: Unknown type converting object to json\n");
		break;
	}

#ifdef PARSER_DEBUG
	tmpstr = json_dumps(newelem, JSON_COMPACT | JSON_ENCODE_ANY);
	DBGPRNT("DEBUG: %sNewly created element for %s is %s\n", indent, parser->json_name, tmpstr);
	free(tmpstr);
#endif
	return newelem;
}


/**
 * @fn c_to_json(uint8_t *data, struct msg_parser *parser)
 * @brief converts c data structure to json string
 * @param data - input data structure to convert to json
 * @param parser - parser tree to guide conversion
 * @param debug - print output json string if true
 * @returns char * string on success holding converted json
 * @returns NULL on failure
 */
char *c_to_json(uint8_t *data, struct msg_parser *parser, bool debug)
{
	char *output = NULL;
	json_t *obj;

	if ((parser == NULL) || (data == NULL)) {
		fprintf(stderr, "ERR: Unable to parse null data\n");
		return NULL;
	}
	/*
	 * do a depth first traversal of the parser, extracting elements from
	 * the data pointer at the proscribed offsets, and using that info to
	 * build json objects
	 */
	obj = create_json_object(data, parser, NULL, " ");

	/*
	 * Now dump the value to a string and return it
	 */
	output = json_dumps(obj, JSON_COMPACT);
	json_decref(obj);

	if (debug) {
		DBGPRNT("DEBUG: C 2 JSON: %s\n", output);
	}

	return output;
}
