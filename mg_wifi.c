// WiFi interface for Mongoose network stack

// Copyright (c) 2024, Jeremy P Bentham
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "ctype.h"
#include "mongoose.h"
#include "mg_wifi.h"
#include "picowi/picowi_defs.h"
#include "picowi/picowi_wifi.h"
#include "picowi/picowi_init.h"
#include "picowi/picowi_join.h"
#include "picowi/picowi_pico.h"
#include "picowi/picowi_ioctl.h"
#include "picowi/picowi_event.h"
#include "picowi/picowi_auth.h"

//#define DEFAULT_AUTH_TYPE   WHD_SECURITY_WPA2_AES_PSK
#define DEFAULT_AUTH_TYPE   WHD_SECURITY_WPA2_WPA_MIXED_PSK
#define DEFAULT_SSID        "testnet"
#define DEFAULT_PASSWD      "testpass"

#define EVENT_POLL_USEC     10000

typedef struct
{
    int val;
    char *str;
} AUTH_TYPES;
AUTH_TYPES auth_types[] = { 
    { 0,                                "OPEN" },
    { WHD_SECURITY_WPA_TKIP_PSK,        "WPA_TKIP_PSK" },
    { WHD_SECURITY_WPA_AES_PSK,         "WPA_AES_PSK" },
    { WHD_SECURITY_WPA_MIXED_PSK,       "WPA_MIXED_PSK" },
    { WHD_SECURITY_WPA2_AES_PSK,        "WPA2_AES_PSK" },
    { WHD_SECURITY_WPA2_TKIP_PSK,       "WPA2_TKIP_PSK" },
    { WHD_SECURITY_WPA2_MIXED_PSK,      "WPA2_MIXED_PSK" },
    { WHD_SECURITY_WPA2_FBT_PSK,        "WPA2_FBT_PSK" },
    { WHD_SECURITY_WPA2_WPA_AES_PSK,    "WPA2_WPA_AES_PSK" },
    { WHD_SECURITY_WPA2_WPA_MIXED_PSK,  "WPA2_WPA_MIXED_PSK" },
    { WHD_SECURITY_WPA3_AES,            "WPA3_AES" },
    { WHD_SECURITY_WPA3_WPA2_PSK,       "WPA3_WPA2_PSK" },
    { -1, "" }
};

uint32_t poll_ticks;
extern EVENT_INFO event_info;
extern uint8_t my_mac[6];
extern bool force_down;

char wifi_ssid[SSID_MAXLEN + 1] = DEFAULT_SSID;
char wifi_passwd[PASSWD_MAXLEN + 1] = DEFAULT_PASSWD;
int wifi_security = DEFAULT_AUTH_TYPE;

// Poll WiFi interface, optionally return incoming data
int wifi_poll(void *buf, size_t buflen)
{
    EVENT_INFO *eip = &event_info;
    int dlen = 0;
    
    if (wifi_get_irq() || ustimeout(&poll_ticks, EVENT_POLL_USEC))
    {
        event_poll();
        join_state_poll(wifi_security, wifi_ssid, wifi_passwd);
        ustimeout(&poll_ticks, 0);
        if (buf && eip->chan == SDPCM_CHAN_DATA && eip->dlen>0 && eip->dlen <= (int)buflen)
        {
            memcpy(buf, eip->data, eip->dlen);
            dlen = eip->dlen;
            eip->dlen = 0;
        }
    }
    return (dlen);
}

// Initialise WiFi interface
static bool mg_wifi_init(struct mg_tcpip_if *ifp) 
{
    set_display_mode(DISPLAY_OPTIONS);
    add_event_handler(join_event_handler);
    if (!wifi_setup())
        printf("Error: SPI communication\n");
    else if (!wifi_init())
        printf("Error: can't initialise WiFi\n");
    else if (!join_start(wifi_security, wifi_ssid, wifi_passwd))
        printf("Error: can't start network join\n");
    else
    {
        ustimeout(&poll_ticks, EVENT_POLL_USEC);
        memcpy(ifp->mac, my_mac, sizeof(my_mac));
        return 1;
    }
    return 0;
}

// Return up/down status of WiFi interface
static bool mg_wifi_up(struct mg_tcpip_if *ifp) 
{
    return (link_check() > 0 && !force_down);
}

// Transmit WiFi data
static size_t mg_wifi_tx(const void *buf, size_t buflen, struct mg_tcpip_if *ifp) 
{
    size_t n = event_net_tx((void *)buf, buflen);
    return  n ? buflen : 0;
}

// Receive WiFi data
static size_t mg_wifi_rx(void *buf, size_t buflen, struct mg_tcpip_if *ifp) 
{
    int n = wifi_poll(buf, buflen);
    return n;
}

struct mg_tcpip_driver mg_tcpip_driver_wifi = { mg_wifi_init, mg_wifi_tx, mg_wifi_rx, mg_wifi_up };

// Return string for an authentication type
char *auth_type_str(int typ)
{
    AUTH_TYPES *atp = auth_types;
    
    while (atp->val >= 0)
    {
        if (typ == atp->val)
            return (atp->str);
        atp++;
    }
    return ("");
}

// Return authentication type for a string, -ve if error
int auth_str_type(char *s)
{
    AUTH_TYPES *atp = auth_types;
    char *t = s;

    while (*t)
    {
        *t = (char)toupper((int)*t);
        *t = *t <= ' ' ? 0 : *t;
        t++;
    }
    while (atp->val >= 0)
    {
        if (!strcmp(atp->str, s))
            return (atp->val);
        atp++;
    }
    return (-1);
}

// Convert IP address to a string
void ip_addr_str(uint ip, char *buff)
{
    mg_snprintf(buff, 16, "%u.%u.%u.%u", (BYTE)(ip >> 24), 
        (BYTE)(ip >> 16), (BYTE)(ip >> 8), (BYTE)ip);
}

// EOF
