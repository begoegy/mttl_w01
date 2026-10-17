#ifndef MTTL_HTTP_FRAMING_H
#define MTTL_HTTP_FRAMING_H
#include <stdint.h>
#include <stddef.h>
/* Complete header only. Rejects duplicate length and all transfer encodings. */
int mttl_http_framing(const uint8_t *,size_t,uint32_t *);
int mttl_http_field(const uint8_t *,size_t,const char *,const uint8_t **,size_t *);
#endif
