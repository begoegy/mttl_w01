#ifndef MTTL_WIFI_FORM_H
#define MTTL_WIFI_FORM_H
#include <stddef.h>
int mttl_wifi_form_type(const char *type);
int mttl_wifi_form(const unsigned char *body,size_t length,char ssid[33],char password[65]);
#endif
