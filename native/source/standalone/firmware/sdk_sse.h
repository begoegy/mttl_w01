#ifndef MTTL_SDK_SSE_H
#define MTTL_SDK_SSE_H
struct httpd_conn;
void mttl_sdk_sse_open(struct httpd_conn *);
void mttl_sdk_sse_pump(void);
void mttl_sdk_sse_close(void);
int mttl_sdk_sse_active(void);
#endif
