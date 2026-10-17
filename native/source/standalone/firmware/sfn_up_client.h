#ifndef SFN_UP_CLIENT_H
#define SFN_UP_CLIENT_H
#include <stddef.h>
#include <stdint.h>
#include "sfn_up_protocol.h"

#define SFN_UP_RX_CAP 256u
#define SFN_UP_TX_CAP 1024u

typedef enum {
    SFN_UP_LINK_OFF=0,
    SFN_UP_LINK_CONNECTING,
    SFN_UP_LINK_ONLINE,
    SFN_UP_LINK_BACKOFF
} sfn_up_link_state;

typedef struct {
    void *context;
    int (*open)(void *context,const char *host,uint16_t port); /* -1 fail, 0 pending, 1 connected */
    int (*close)(void *context);                               /* -1 fail, 1 closed */
    int (*send)(void *context,const uint8_t *data,size_t size);/* -1 fail, 0 would block */
    int (*receive)(void *context,uint8_t *data,size_t size);   /* -1 closed/fail, 0 would block */
} sfn_up_io;

typedef size_t (*sfn_up_command_handler)(void *context,const sfn_up_command *command,char *response,size_t cap);

typedef struct {
    sfn_up_link_state state;
    uint8_t rx[SFN_UP_RX_CAP];
    uint8_t tx[SFN_UP_TX_CAP];
    uint16_t rx_size,tx_size,tx_sent;
    uint32_t backoff_at;
    uint32_t connected_at;
    uint32_t reconnects;
    uint32_t commands;
    uint32_t rejected;
    uint32_t io_failures;
    uint8_t backoff_level;
    uint8_t boot_sent;
} sfn_up_client;

void sfn_up_client_init(sfn_up_client *client);
void sfn_up_client_reset(sfn_up_client *client,const sfn_up_io *io);
int sfn_up_client_step(sfn_up_client *client,const sfn_up_io *io,int network_online,
                       const char *host,uint16_t port,uint32_t now,
                       const char *model,const char mac[13],const char *firmware,
                       sfn_up_command_handler handler,void *handler_context);
int sfn_up_client_publish(sfn_up_client *client,const char *line,size_t size);
int sfn_up_client_ready(const sfn_up_client *client);

#endif
