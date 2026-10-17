/* Explicit unsupported-feature errors for OFFLINE HTTP-only experiment. */
#include <stddef.h>
#include <stdint.h>
int httpd_tls_setup_init(char *cert, char *key, char *ca) {
    (void)cert; (void)key; (void)ca; return -1;
}
void httpd_tls_setup_free(void) {}
void *httpd_tls_new_handshake(int *socket, uint8_t secure) {
    (void)socket; (void)secure; return NULL;
}
void httpd_tls_free(void *tls) { (void)tls; }
void httpd_tls_close(void *tls) { (void)tls; }
int httpd_tls_read(void *tls, uint8_t *buf, size_t length) {
    (void)tls; (void)buf; (void)length; return -1;
}
int httpd_tls_write(void *tls, uint8_t *buf, size_t length) {
    (void)tls; (void)buf; (void)length; return -1;
}
/* Basic authentication is unsupported in this capacity experiment. */
int httpd_base64_encode(uint8_t *data, size_t length, char *out, size_t capacity) {
    (void)data; (void)length;
    if (out && capacity) out[0] = 0;
    return -1;
}
