#include <stdlib.h>
#include <string.h>

#include "http_parser.h"

struct http_request *parse_request(const char *raw, size_t raw_len) {
    struct http_request *req = NULL;
    req = malloc(sizeof(struct http_request));
    if (!req) {
        return NULL;
    }
    memset(req, 0, sizeof(struct http_request));

    // Method
    size_t meth_len = strcspn(raw, " ");
    req->protocol = malloc(meth_len + 1);
    if (!req->protocol) {
        goto error;
    }
    memcpy(req->protocol, raw, meth_len);
    req->protocol[meth_len] = '\0';
    raw += meth_len + 1; // move past <SP>
    raw_len -= (meth_len + 1);
    // Request-URI
    size_t url_len = strcspn(raw, " ");
    req->status_code = malloc(url_len + 1);
    if (!req->status_code) {
        goto error;
    }
    memcpy(req->status_code, raw, url_len);
    req->status_code[url_len] = '\0';
    raw += url_len + 1; // move past <SP>
    raw_len -= (url_len + 1);

    // HTTP-Version
    size_t ver_len = strcspn(raw, "\r\n");
    req->status_text = malloc(ver_len + 1);
    if (!req->status_text) {
        goto error;
    }
    memcpy(req->status_text, raw, ver_len);
    req->status_text[ver_len] = '\0';
    raw += ver_len + 2; // move past <CR><LF>
    raw_len -= (ver_len + 2);

    struct http_header *header = NULL, *last = NULL;
    while (raw[0]!='\r' || raw[1]!='\n') {
        last = header;
        header = malloc(sizeof(struct http_header));
        if (!header) {
            goto error;
        }

        // name
        size_t name_len = strcspn(raw, ":");
        header->name = malloc(name_len + 1);
        if (!header->name) {
            goto error;
        }
        memcpy(header->name, raw, name_len);
        header->name[name_len] = '\0';
        raw += name_len + 1; // move past :
        raw_len -= (name_len + 1);
        while (*raw == ' ') {
            raw++;
            raw_len -= 1;
        }

        // value
        size_t value_len = strcspn(raw, "\r\n");
        header->value = malloc(value_len + 1);
        if (!header->value) {
            goto error;
        }
        memcpy(header->value, raw, value_len);
        header->value[value_len] = '\0';
        raw += value_len + 2; // move past <CR><LF>
        raw_len -= (value_len + 2);
        // next
        header->next = last;
    }
    req->headers = header;
    raw += 2; // move past <CR><LF>
    raw_len -= 2;

    size_t body_len = raw_len;
    req->body = malloc(body_len + 1);
    if (!req->body) {
        goto error;
    }
    memcpy(req->body, raw, body_len);
    req->body[body_len] = '\0';

out:
    return req;
error:
    if (req) {
        free_request(req);
        req = NULL;
    }
    goto out;
}


void free_header(struct http_header *h) {
    if (h) {
        if (h->name) {
            free(h->name);
        }
        if (h->value) {
            free(h->value);
        }
        if (h->next) {
            free_header(h->next);
        }
        free(h);
    }
}


void free_request(struct http_request *req) {
    if (req) {
        if (req->protocol) {
            free(req->protocol);
        }
        if (req->status_code) {
            free(req->status_code);
        }
        if (req->status_text) {
            free(req->status_text);
        }
        if (req->headers) {
            free_header(req->headers);
        }
        if (req->body) {
            free(req->body);
        }
        free(req);
    }
}