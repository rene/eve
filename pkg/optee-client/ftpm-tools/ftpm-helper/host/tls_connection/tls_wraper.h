#ifndef __TLS_WRAPER_H
#define __TLS_WRAPER_H

#include <stdio.h>
#include <stdint.h>
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>

#define MAX_REQUEST_SIZE      1024

typedef struct tls_client_config_s {
    const char *server_name;    /* hostname of the server (client only)     */
    const char *server_port;    /* port on which the ssl service runs       */
	unsigned char *ca_buff;        /* the path with the CA certificate(s) reside */
    size_t ca_len;
    unsigned char *crt_buff;       /* the file with the client certificate     */
    size_t crt_len;
    unsigned char *key_buff;       /* the file with the client key             */
    size_t key_len;
    uint32_t read_timeout;       /* milisecond, set 0 for no time out,  */
} tls_client_config;

int tls_init(tls_client_config *tls_client, mbedtls_ssl_context *ssl, mbedtls_net_context *server_fd);
int tls_connect(tls_client_config *tls_client, mbedtls_ssl_context *ssl, mbedtls_net_context *server_fd);
int tls_send(mbedtls_ssl_context *ssl, uint8_t *data, size_t datalen);
int tls_read(mbedtls_ssl_context *ssl, uint8_t **data, size_t *datalen);
int tls_close(mbedtls_ssl_context *ssl);

#endif /*__TLS_WRAPER_H*/