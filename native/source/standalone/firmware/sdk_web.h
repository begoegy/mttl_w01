#ifndef MTTL_SDK_WEB_H
#define MTTL_SDK_WEB_H
#ifdef __cplusplus
extern "C" {
#endif
/* Offline-linked read-only landing page, not the full dashboard. */
int mttl_sdk_web_start(void);
/* SDK stop is not yet proven to be a complete task-join barrier. */
void mttl_sdk_web_stop(void);
int mttl_sdk_web_stopped(void);
#ifdef __cplusplus
}
#endif
#endif
