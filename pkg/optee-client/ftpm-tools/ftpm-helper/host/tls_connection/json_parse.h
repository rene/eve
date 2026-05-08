#ifndef __JSON_PARSE_H
#define __JSON_PARSE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <jansson.h>

#define ARRAY_SIZE(X) (sizeof(X) / sizeof((X)[0]))

/**
 * @def ALIGN_SIZE
 * @brief the defined alignment size for all our strucutres.  sizeof(void *)
 * should be enough on most processors
 */
#define ALIGN_SIZE sizeof(void *)

/**
 * @def __ALIGN_MASK(x, mask)
 * @brief aligns value x to the next largest mask boundary
 * @param x - value to align
 * @param mask - value boundary to align to
 */
#define __ALIGN_MASK(x, mask) (((x) + (mask)) & ~(mask))

/**
 * @def ALIGN
 * @brief aligns values to alignmnet boundaries
 * @param x - value to align
 * @param a - boundary value
 */
#define ALIGN(x, a) __ALIGN_MASK(x, (typeof(x))(a) - 1)

/**
 * @def MSG_STRING(jname, maxinlen, pstruct, member)
 * @brief defines a string element in a data structure to parse
 * @param jname - the json name to encode to
 * @param maxinlen - maximum length of the string
 * @param pstruct - the parent struct that the string is embedded in
 * @param member - the c member name within the parent struct
 */
#define MSG_STRING(jname, maxinlen, pstruct, member) { \
		.json_name = jname, \
		.obj_type = JSON_STRING, \
		.offset = offsetof(pstruct, member), \
		.dynamic = false, \
		.anon = false, \
		.realsize = maxinlen, \
		.alignsize = ALIGN(maxinlen, ALIGN_SIZE), \
		.num_children = 0, \
		.children = NULL \
}

/**
 * @def MSG_DYNAMIC_STRING(jname, maxinlen, pstruct, member)
 * @brief defines a dynamic string element in a data structure to parse
 * @param jname - the json name to encode to
 * @param maxinlen - maximum length of the string
 * @param pstruct - the parent struct that the string is embedded in
 * @param member - the c member name within the parent struct
 */
#define MSG_DYNAMIC_STRING(jname, pstruct, member) { \
		.json_name = jname, \
		.obj_type = JSON_STRING, \
		.offset = offsetof(pstruct, member), \
		.realsize = 0, \
		.alignsize = 0, \
		.dynamic = true, \
		.anon = false, \
		.num_children = 0, \
		.children = NULL \
}

#define MSG_ANON_STRING() { \
		.json_name = NULL, \
		.obj_type = JSON_STRING, \
		.offset = sizeof(char *), \
		.realsize = sizeof(char *), \
		.alignsize = sizeof(char *), \
		.dynamic = true, \
		.anon = true, \
		.num_children = 0, \
		.children = NULL \
}

#define MSG_ANON_INTEGER() { \
		.json_name = NULL, \
		.obj_type = JSON_INTEGER, \
		.offset = sizeof(int), \
		.realsize = sizeof(int), \
		.alignsize = sizeof(int), \
		.dynamic = true, \
		.anon = true, \
		.num_children = 0, \
		.children = NULL \
}

/**
 * @def MSG_INTEGER(jname, pstruct, member)
 * @brief defines an integer element in a data structure to parse
 * @param jname - the json name to encode to
 * @param pstruct - the parent struct that the integer is embedded in
 * @param member - the c member name within the parent struct
 */
#define MSG_INTEGER(jname, pstruct, member) { \
		.json_name = jname, \
		.obj_type = JSON_INTEGER, \
		.offset = offsetof(pstruct, member), \
		.realsize = sizeof(int), \
		.dynamic = false, \
		.anon = false, \
		.alignsize = ALIGN(sizeof(int), ALIGN_SIZE), \
		.num_children = 0, \
		.children = NULL \
}

/**
 * @def MSG_INTEGER64(jname, pstruct, member)
 * @brief defines a 64-bit integer element in a data structure to parse
 * @param jname - the json name to encode to
 * @param pstruct - the parent struct that the integer is embedded in
 * @param member - the c member name within the parent struct
 */
#define MSG_INTEGER64(jname, pstruct, member) { \
		.json_name = jname, \
		.obj_type = JSON_INTEGER, \
		.offset = offsetof(pstruct, member), \
		.realsize = sizeof(uint64_t), \
		.alignsize = ALIGN(sizeof(uint64_t), ALIGN_SIZE), \
		.dynamic = false, \
		.anon = false, \
		.num_children = 0, \
		.children = NULL \
}

/**
 * @def MSG_BOOL(jname, pstruct, member)
 * @brief defines a boolean element in a data structure to parse
 * @param jname - the json name to encode to
 * @param pstruct - the parent struct we are embedded in
 * @param member - the c  member name within the parent struct
 */
#define MSG_BOOL(jname, pstruct, member) { \
		.json_name = jname, \
		.obj_type = JSON_TRUE, \
		.offset = offsetof(pstruct, member), \
		.realsize = sizeof(bool), \
		.alignsize = ALIGN(sizeof(bool), ALIGN_SIZE), \
		.dynamic = false, \
		.anon = false, \
		.num_children = 0, \
		.children = NULL \
}

/**
 * @def MSG_ARRAY(jname, maxinlen, pstruct, member, child)
 * @brief defines an array element in a data structure to parse
 * @param jname - the json name to encode to
 * @param maxinlen - maximum length of the array in elements
 * @param pstruct - the parent struct that the array is embedded in
 * @param member - the c member name within the parent struct
 * @param child - parser for the element type the array holds
 */
#define MSG_ARRAY(jname, maxelem, pstruct, member, child) { \
		.json_name = jname, \
		.obj_type = JSON_ARRAY, \
		.offset = offsetof(pstruct, member), \
		.realsize = maxelem, \
		.alignsize = 0, \
		.num_children = 1, \
		.dynamic = false, \
		.anon = false, \
		.children = child \
}

/**
 * @def MSG_DYNAMIC_ARRAY(jname, ecountname, pstruct, member, child)
 * @brief defines a dynamic array element in a data structure to parse
 * @param jname - the json name to encode to
 * @param ecountname - member of the struct that hold the number of elements in
 * the array
 * @param pstruct - the parent struct that the array is embedded in
 * @param member - the c member name within the parent struct
 * @param child - parser for the element type the array holds
 */
#define MSG_DYNAMIC_ARRAY(jname, ecountname, pstruct, member, child) { \
		.json_name = jname, \
		.obj_type = JSON_ARRAY, \
		.dynamic = true, \
		.anon = false, \
		.realsize = 0, \
		.alignsize = 0, \
		.acoffset = offsetof(pstruct, ecountname), \
		.offset = offsetof(pstruct, member), \
		.num_children = 1, \
		.children = child \
}

/**
 * @def MSG_OBJECT(jname, maxinlen, pstruct, member, children)
 * @brief defines a struct element in a data structure to parse
 * @param jname - the json name to encode to
 * @param maxinlen - maximum length of the struct
 * @param pstruct - the parent struct that the struct is embedded in
 * @param member - the c member name within the parent struct
 * @param children - array of child parsers for struct elements
 */
#define MSG_OBJECT(jname, pstruct, member, kids) { \
		.json_name = jname, \
		.obj_type = JSON_OBJECT, \
		.offset = offsetof(pstruct, member), \
		.realsize = 0, \
		.alignsize = 0, \
		.num_children = ARRAY_SIZE(kids), \
		.dynamic = false, \
		.anon = false, \
		.children = kids \
}

/**
 * @def MSG_ARRAY_OBJ(children)
 * @brief defines a struct element in a array data structure to parse
 * @param children - array of child parsers for struct elements
 */
#define MSG_ARRAY_OBJ(member, kids) { \
		.json_name = "unnamed", \
		.obj_type = JSON_OBJECT, \
		.offset = sizeof(member), \
		.realsize = sizeof(member), \
		.alignsize = sizeof(member), \
		.num_children = ARRAY_SIZE(kids), \
		.dynamic = false, \
		.anon = false, \
		.children = kids \
}

/**
 * @def MSG_ARRAY_INTEGER()
 * @brief defines a struct element in a array data structure to parse
 */
#define MSG_ARRAY_INTEGER() { \
		.json_name = "unnamed", \
		.obj_type = JSON_INTEGER, \
		.offset = sizeof(int), \
		.realsize = sizeof(int), \
		.dynamic = false, \
		.anon = false, \
		.alignsize = ALIGN(sizeof(int), ALIGN_SIZE), \
		.num_children = 0, \
		.children = NULL \
}

/**
 * @def _MSG_TOPLEVEL(size, kids)
 * @brief defines the top level parser for an object
 * @param name - name of the toplevel parser
 * @param size - maximum length of the object
 * @param kids - the child parsers for the struct
 */
#define MSG_TOPLEVEL(name, size, kids) struct msg_parser name = { \
		.json_name	= NULL, \
		.obj_type	= JSON_OBJECT, \
		.offset		= 0, \
		.realsize	= size, \
		.alignsize	= ALIGN(size,	  ALIGN_SIZE), \
		.num_children	= ARRAY_SIZE(kids), \
		.dynamic	= false, \
		.anon		= false, \
		.children	= kids \
}

typedef void *msg_obj;

struct msg_parser {
	char *			json_name;
	int			obj_type;       /* taken from libjansson.h json types */
	size_t			offset;         /* offset of the element in a struct */
	size_t			realsize;       /*max length of element, or number of elements in array*/
	size_t			alignsize;      /* size of an element plus any padding the compiler needs to add */
	bool			dynamic;        /* indicate that this parser is an element for a dynamic variable */
	bool			anon;           /* indicate that this parser is an anonymous variable */
	size_t			acoffset;       /* if the parser is dynamic, and the obj_type is JSON_ARRAY, then this points to the location of the array count variable, which must be a size_t */
	size_t			num_children;
	struct msg_parser *	children;
};

/**
 * @file
 * @brief this api defines the registration methods we use for registering
 * paths to publish on and subscribe to for our comms channel to the cloud
 */

void msg_obj_put(void *cdata);

char *c_to_json(uint8_t *data, struct msg_parser *parser, bool debug);

void *json_to_c(char *inbuf, size_t insz, struct msg_parser *parser);

#endif /*__JSON_PARSE_H*/