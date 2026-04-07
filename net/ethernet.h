#ifndef ETHERNET_H
#define ETHERNET

#include "network.h"

int ethernet_init(void);
int ethernet_send_frame(struct ethernet_frame* frame);
void ethernet_rx_callback(struct ethernet_frame* frame); // called by driver

#endif
