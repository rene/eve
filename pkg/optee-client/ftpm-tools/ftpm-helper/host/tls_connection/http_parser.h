#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

struct http_header {
    char *name;
    char *value;
    struct http_header *next;
};

struct http_request {
    char *protocol;
    char *status_code;
    char *status_text;
    struct http_header *headers;
    char *body;
};


struct http_request *parse_request(const char *raw, size_t);
void free_header(struct http_header *h);
void free_request(struct http_request *req);

#endif //HTTP_PARSER_H