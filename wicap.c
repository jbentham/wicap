// Pi Pico fast data capture over WiFi, see https://iosoft.blog/wicap
//
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

// v0.01 JPB 2/5/23  Adapted from pedla.c v0.66
// v0.02 JPB 22/4/24 Added PIO code so data pins can be read using DMA
// v0.03 JPB 23/4/24 Corrected DMA shift direction, dump 16 DMA samples
// v0.04 JPB 14/5/24 Use http fetch in place of XMLHttpRequest in rpscope.html
// v0.05 JPB 14/5/24 Moved status text to button line
//                   Set ip address using remip HTTP parameter
// v0.06 JPB 21/5/24 Adapted to use latest version of Mongoose
// v0.07 JPB 21/5/24 Added configuration data, and flash memory write
// v0.08 JPB 22/5/24 Simplified IP address entry in config
// v0.09 JPB 22/5/24 Added data fetch to rpscope
// v0.10 JPB 26/5/24 Display remote IP address of GET
//                   Clear command value after single capture
// v0.11 JPB 27/5/24 Eliminated unused capture states
// v0.12 JPB 4/6/24  Added capture state machine to rpscope.html
// v0.13 JPB 4/6/24  Added base64 data fetch
// v0.14 JPB 4/6/24  Initial scope display
// v0.15 JPB 18/6/24 Corrected browser data fetch when capturing
//                   Added error state when fetch failed
// v0.16 JPB 4/7/24  Added XSAMP_PRE to discard first 20 samples
// v0.17 JPB 4/7/24  Removed unnecessary HTML table
//                   Added capture parameters
// v0.18 JPB 8/7/24  Added logic analyser plot
// v0.19 JPB 9/7/24  Added display mode selection
// v0.20 JPB 9/7/24  Simplified analogue scaling
// v0.21 JPB 14/7/24 Corrected xrate setting
// v0.22 JPB 15/7/24 Added binary file transfer
// v0.23 JPB 15/7/24 For binary data, Use XMLHttpRequest in place of fetch
//                   Corrected state machine error handling
// v0.24 JPB 16/7/24 Added stop command, to stop a slow capture
// v0.25 JPB 11/8/24 Added analog sensitivity settings to display mode

#define SW_VERSION  "0.25"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "picowi/picowi_defs.h"
#include "picowi/picowi_auth.h"
#include "mg_wifi.h"
#include "mongoose.h"
#include "picocap.h"

// Web server
#define LISTEN_URL          "http://0.0.0.0:80"
#define ROOT_FILENAME       "/"
#define LA_FNAME_BASE64     "/data.txt"
#define LA_FNAME_BIN        "/data.bin"
#define STATUS_FILENAME     "/status.txt"
#define BASE64_SEG_SIZE     720
#define MAX_DATALEN         ((BASE64_SEG_SIZE*4)/3)
#define BASE64_TEXTBLOCKLEN 24
#define BASE64_BINBLOCKLEN  ((BASE64_TEXTBLOCKLEN*3)/4)

// Timeout values in msec
#define LINK_UP_BLINK       500
#define LINK_DOWN_BLINK     100
#define JOIN_DOWN_MS        3000   

// Maximum number of simultaneous TCP connections
#define MAXCONNS            8

// HTML header to disable client caching
#define NO_CACHE "Cache-Control: no-cache, no-store, must-revalidate\r\nPragma: no-cache\r\nExpires: 0\r\n"
#define ALLOW_CORS "Access-Control-Allow-Origin: *\r\n"
#define TEXT_PLAIN "Content-Type: text/plain\r\n"
    
// Structure to hold parameters for an open file
typedef struct {
    uint outpos, outlen, inpos, inlen, index, millis;
    bool inuse, base64;
} FILESTRUCT;

FILESTRUCT filestructs[MAXCONNS];
bool force_down;
extern struct mg_fs mg_test_fs;
int startval;
char base64_textblock[BASE64_TEXTBLOCKLEN+1];
char version[] = "WiCap v" SW_VERSION;

uint ready_ticks, led_ticks;
char temps[TEMPS_SIZE];
const char base64_chars[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

extern SERVER_PARAM server_params[];
extern WORD samples[XSAMP_MAX];

void serial_init(void);
void listener(struct mg_connection *c, int ev, void *ev_data);
void web_get_params(struct mg_http_message *hm, SERVER_PARAM *args, int *cmdp);
int json_status(char *buff, int maxlen, int typ);
void fs_check(void);

int main(void) 
{
    bool ledon = 0;
    struct mg_mgr mgr;
    struct mg_tcpip_if mif = {.driver = &mg_tcpip_driver_wifi, .mgr = &mgr};
    
    set_sys_clock_khz(PWM_CLOCK/1000, true);
    stdio_init_all();
    serial_init();
    cap_init();
    cap_pout_init(PIN_SCK, get_param_int(ARG_XRATE));
    //cap_start(0, 0);
    xprintf("\n%s\n", version);
    //xprintf("Flash size %u\n", flash_size());

    if (get_param_int(ARG_IP_BASE))
    {
        mif.ip = mg_htonl(get_param_int(ARG_IP_BASE) + get_param_int(ARG_UNIT));
        mif.mask = mg_htonl(MG_U32(255, 255, 255, 0));
        mif.gw = mg_htonl(get_param_int(ARG_GATEWAY));
        ip_addr_str(get_param_int(ARG_IP_BASE) + get_param_int(ARG_UNIT), temps);
        xprintf("Using static IP %s\n", temps);
    }
    else
        xprintf("Using dynamic IP (DHCP)\n");
    mg_mgr_init(&mgr);
    mg_log_set(MG_LL_NONE);
    mg_tcpip_init(&mgr, &mif);
    mg_http_listen(&mgr, LISTEN_URL, listener, &mgr);
    //la_set_led(0);
    for (;;) 
    {
        mg_mgr_poll(&mgr, 2);
        if (mstimeout(&led_ticks, link_check() > 0 ? LINK_UP_BLINK : LINK_DOWN_BLINK))
        {
            wifi_set_led(ledon = !ledon);
        }
        if (get_param_int(ARG_STATE)>STATE_READY && !cap_capturing())
        {
            cap_end();
            cap_set_state(STATE_READY);
        }
        if (!mif.driver->up(0))
            wifi_poll(0, 0);
    }
    return 0;
}

// Initialise serial console interface
// Console UART is set using compiler definition -DPICO_DEFAULT_UART=0 or 1
void serial_init(void)
{
    uart_init(UART_HW, UART_BAUD);
    gpio_set_pulls(PIN_SER_RX, 1, 0);
    gpio_set_function(PIN_SER_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_SER_RX, GPIO_FUNC_UART);
}

// Return non-zero if timeout
bool mstimeout(uint *tickp, uint msec)
{
    uint t = mg_millis();
    uint dt = t - *tickp;

    if (msec == 0 || dt >= msec)
    {
        *tickp = t;
        return (1);
    }
    return (0);
}

// Return char count of base64-encoded string, given original length
// If len is not divisible by 3, add 2 chars + 2 pad, or 3 chars + 1 pad
int bin_base64len(int dlen)
{
    return (dlen / 3) * 4 + (dlen % 3 ? 4 : 0);
}

// Encode data into base64 (3 bytes -> 4 chars)
int base64_enc(void *inp, int inlen, void *outp)
{
    int olen=0, val, n=inlen/3, extra=inlen%3;
    BYTE *ip = (BYTE *)inp;
    char *op = (char *)outp;
    
    while (n--)
    {
        val =  (int)*ip++ << 16;
        val += (int)*ip++ << 8;
        val += (int)*ip++;
        op[olen++] = base64_chars[(val >> 18) & 0x3F];
        op[olen++] = base64_chars[(val >> 12) & 0x3F];
        op[olen++] = base64_chars[(val >> 6)  & 0x3F];
        op[olen++] = base64_chars[val         & 0x3F];
    }
    if (extra)
    {
        val = (int)*ip++ << 16;
        if (extra > 1)
            val += (int)*ip++ << 8;
        op[olen++] = base64_chars[(val >> 18) & 0x3F];
        op[olen++] = base64_chars[(val >> 12) & 0x3F];
        if (extra > 1)
            op[olen++] = base64_chars[(val >> 6) & 0x3F];
        while (extra++ < 3)
            op[olen++] = '=';
    }
    return (olen);
}

// Return descriptive string if state has changed
char *state_change(int state)
{
    static int last_state = 0;
    static char *states[] = { "DOWN", "UP", "REQ", "READY" };

    if (state != last_state && state >= MG_TCPIP_STATE_DOWN && state <= MG_TCPIP_STATE_READY)
    {
        last_state = state;
        return states[state];
    }
    return 0;
}

// Functions providing a file-like interface for analyser data

// Return file list (not implemented)
static void fs_list(const char *path, void(*fn)(const char *, void *), void *userdata)
{
    (void) path, (void) fn, (void) userdata;
}
// Write file (not implemented)
static size_t fs_write(void *fd, const void *buf, size_t len) 
{
    (void) fd, (void) buf, (void) len;
    return 0;
}
// Rename file (not implemented)
static bool fs_rename(const char *from, const char *to) 
{
    (void) from, (void) to;
    return false;
}
// Remove file (not implemented)
static bool fs_remove(const char *path) 
{
    (void) path;
    return false;
}
// Make directory (not implemented)
static bool fs_mkdir(const char *path) 
{
    (void) path;
    return false;
}

// Return status of logic analyser base64 file interface
static int fs_stat_base64(const char *path, size_t *size, time_t *mtime)
{
    static time_t t = 0;
    if (size)
    {
        //*size = bin_base64len(caparams.nsamp * 2);
        *size = bin_base64len(get_param_int(ARG_NSAMP) * 2);
        xprintf("Stat  file %s size %u\n", path, *size);
    }
    if (mtime)
        *mtime = t++;
    return (get_param_int(ARG_NSAMP) * 2);
}

// Return status of logic analyser binary file interface
static int fs_stat_bin(const char *path, size_t *size, time_t *mtime)
{
    static time_t t = 0;
    if (size)
    {
        *size = get_param_int(ARG_NSAMP) * 2;
        xprintf("Stat  file %s size %u\n", path, *size);
    }
    if (mtime)
        *mtime = t++;
    return (get_param_int(ARG_NSAMP) * 2);
}

// Start analyser file transfer, binary mode
static void *fs_open_bin(const char *path, int flags) 
{
    (void) flags;
    if (strstr(path, ".gz"))
        return NULL;
    for (int i = 0; i < MAXCONNS; i++)
    {
        if (!filestructs[i].inuse) 
        {
            FILESTRUCT *fptr = &filestructs[i];
            fptr->inuse = true;
            fptr->base64 = false;            
            fptr->outlen = fptr->inlen = get_param_int(ARG_NSAMP) * 2;
            fptr->outpos = fptr->inpos = 0;
            fptr->index = i;
            fptr->millis = mg_millis();
            xprintf("Open  file %u %s\n", fptr->index, path);
            return(fptr);
        }
    }
    return(NULL);
}

// Start analyser file transfer, base64 mode
static void *fs_open_base64(const char *path, int flags) 
{
    FILESTRUCT *fptr = (FILESTRUCT *)fs_open_bin(path, flags);
    
    if (fptr)
    {
        fptr->outlen = bin_base64len(get_param_int(ARG_NSAMP) * 2);
        fptr->base64 = 1;
    }
    return (void *)fptr;
}

// Close file
static void fs_close(void *fp) 
{
    FILESTRUCT *fptr = fp;
    
    uint dt = mg_millis() - fptr->millis;
    uint speed = dt ? (fptr->outlen * 1000) / dt : 0;
    xprintf("Close file %u, %u msec, %u of %u bytes, %u bytes/sec\n", 
        fptr->index, dt, fptr->outpos, fptr->outlen, speed);
    fptr->outpos = fptr->outlen = fptr->inuse = 0;
    base64_textblock[0] = 0;
    startval += XSAMP_DEFAULT / 100;
}

// Check for old (unused) file pointers
void fs_check(void) 
{
    for (int i = 0; i < MAXCONNS; i++)
    {
        FILESTRUCT *fptr = &filestructs[i];
        if (fptr->inuse && mg_millis() - fptr->millis > 30000) 
        {
            fptr->outpos = fptr->outlen = fptr->inuse = 0;
            xprintf("Closing unused file transfer\n");
        }
    }
}

// Return next block of binary data
int fs_bin_data(FILESTRUCT *fptr, void *buf, int len)
{
    uint8_t *dp = (uint8_t *) &samples[XSAMP_PRE];    
    //uint8_t *dp = (uint8_t *) samples;
    if (len > 0)
        memcpy(buf, &dp[fptr->inpos], len);
    return (len);
} 
    
// Read data stream, returning base64 encoded data
static size_t fs_read_base64(void *fd, void *buf, size_t length) 
{
    FILESTRUCT *fptr = fd;
    int outlen = 0, end = (fptr->outpos + length >= fptr->outlen);
    
    if (length && length < BASE64_TEXTBLOCKLEN && !base64_textblock[0])
    {
        fs_bin_data(fptr, temps, BASE64_BINBLOCKLEN);
        base64_enc(temps, end ? fptr->inlen-fptr->inpos : BASE64_BINBLOCKLEN, base64_textblock);
        fptr->inpos += BASE64_BINBLOCKLEN;
    }        
    if (length && base64_textblock[0])
    {
        int n = fptr->outpos % BASE64_TEXTBLOCKLEN;
        outlen = MIN((int)length, (int)BASE64_TEXTBLOCKLEN - n);
        memcpy(buf, &base64_textblock[n], outlen);
        if (n + outlen >= BASE64_TEXTBLOCKLEN || end)
            base64_textblock[0] = 0;
    }
    else
    {
        outlen = MIN(MAX_DATALEN, length);
        outlen -= outlen % BASE64_TEXTBLOCKLEN;
        int inlen = (outlen * 3) / 4;
        fs_bin_data(fptr, temps, inlen);
        fptr->inpos += inlen;
        base64_enc(temps, inlen, buf);
    }
#if DISP_BLOCKS    
    xprintf("Read  file %u req %4d dlen %4d pos %d len %d\n", fptr->index, length, outlen, fptr->outpos, fptr->len);
#endif    
    fptr->outpos += outlen;
    return(outlen);
}

// Read data stream, returning binary data
static size_t fs_read_bin(void *fd, void *buf, size_t length) 
{
    FILESTRUCT *fptr = fd;
    int outlen = MIN(fptr->outlen - fptr->outpos, length);
    
    fs_bin_data(fptr, buf, outlen);
#if DISP_BLOCKS    
    xprintf("Read  file %u req %4d dlen %4d pos %d len %d\n", 
        fptr->index, length, outlen, fptr->outpos, fptr->len);
#endif
    fptr->inpos += outlen;
    fptr->outpos += outlen;
    return (outlen);
}

// Move file pointer
static size_t fs_seek(void *fd, size_t offset) 
{
    (void) fd, (void) offset;
    FILESTRUCT *fptr = fd;
    xprintf("Seek  file %u offset %d\n", fptr->index, offset);
    fptr->outpos = MIN(fptr->outlen, offset);
    return(fptr->outpos);
}

// Pointers to logic analyser base64 file functions
struct mg_fs mg_fs_base64 = 
{
    fs_stat_base64,  fs_list,  fs_open_base64,  fs_close, fs_read_base64,
    fs_write,  fs_seek, fs_rename, fs_remove, fs_mkdir
 };

// Pointers to logic analyser binary file functions
struct mg_fs mg_fs_bin = 
{
    fs_stat_bin,  fs_list,  fs_open_bin,  fs_close, fs_read_bin,
    fs_write,  fs_seek, fs_rename, fs_remove, fs_mkdir
 };

// Connection callback
//void listener(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
void listener(struct mg_connection *c, int ev, void *ev_data)
{
    struct mg_mgr *mgrp = c->mgr;
    //struct mg_mgr *mgrp = (struct mg_mgr *)fn_data;
    struct mg_tcpip_if *ifp = mgrp->priv;
    struct mg_http_serve_opts opts = {.extra_headers = NO_CACHE ALLOW_CORS};

    if (ev == MG_EV_HTTP_MSG) 
    {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        int len = hm->method.len + hm->uri.len + hm->query.len + 2, cmd=-1;
        
        xprintf("%M ", mg_print_ip, &c->rem);
        for (int i = 0; i < len; i++)
            putchar(hm->method.buf[i]);
        putchar('\n');

        web_get_params(hm, server_params, &cmd);
        if (cmd >= 0)
        {
            xprintf("Command %d\n", cmd);
            if (cmd == CMD_SINGLE || cmd == CMD_MULTI)
            {
                cap_set_state(STATE_CAPTURING);
                int n = MIN(get_param_int(ARG_XSAMP), XSAMP_MAX) + XSAMP_PRE;
                cap_start(samples, n);
            }
            if (cmd == CMD_SINGLE)
                set_param_int(ARG_CMD, 0);
            else if (cmd == CMD_STOP)
            {
                cap_set_state(STATE_ERROR);
                cap_end();
            }
        }
        if (mg_match(hm->uri, mg_str(ROOT_FILENAME), NULL))
        {
            mg_http_reply(c, 200, TEXT_PLAIN ALLOW_CORS, "%s", version); 
        }
        else if (mg_match(hm->uri, mg_str(STATUS_FILENAME), NULL))
        {
            json_status(temps, sizeof(temps) - 1, 0);
            //xprintf("%s\n", temps);
            mg_http_reply(c, 200, NO_CACHE ALLOW_CORS, "%s", temps); 
        }
        else if (mg_match(hm->uri, mg_str(LA_FNAME_BASE64), NULL))
        {
            opts.fs = &mg_fs_base64;
            mg_http_serve_dir(c, hm, &opts);
            c->is_draining = 1;
        }
        else if (mg_match(hm->uri, mg_str(LA_FNAME_BIN), NULL))
        {
            opts.fs = &mg_fs_bin;
            mg_http_serve_dir(c, hm, &opts);
            c->is_draining = 1;
        }
        else 
        {
            mg_http_reply(c, 404, "", "Not Found\n");
        }
    }
    else 
    {
        char *s = state_change(ifp->state);
        if (s) 
        {
            if (ifp->state == MG_TCPIP_STATE_READY)
                xprintf("IP state: %s, IP: %M, GW: %M\n", s, mg_print_ip4, &ifp->ip, mg_print_ip4, &ifp->gw);
            else
                xprintf("IP state: %s\n", s);
            mstimeout(&ready_ticks, 0);
        }
        else if (ifp->state != MG_TCPIP_STATE_READY && mstimeout(&ready_ticks, JOIN_DOWN_MS))
            force_down = !force_down;
    }
}

// Get HTTP query parameter values, including a command value (if present)
void web_get_params(struct mg_http_message *hm, SERVER_PARAM *args, int *cmdp)
{
    while (args->type)
    {
        if (args->type==ARG_VAL_T && 
            mg_http_get_var(&hm->query, args->name, temps, sizeof(temps)) > 0)
            args->val = strtol(temps, NULL, 10);
        else if (args->type == ARG_CMD_T && cmdp &&
            mg_http_get_var(&hm->query, args->name, temps, sizeof(temps)) > 0)
            *cmdp = args->val = strtol(temps, NULL, 10);
        args++;
    }
}

// Return server status as json string
int json_status(char *buff, int maxlen, int typ) 
{
    SERVER_PARAM *arg = server_params;
    int i = 0, n = sprintf(buff, "{");
    
    while (arg[i].name[0] && n < maxlen - 20)
    {
        if ((typ == 0 && i < ARG_SECURITY) || arg[i].type == typ)
            n += sprintf(&buff[n], "%s\"%s\":%d", n > 2 ? "," : "", 
                arg[i].name, arg[i].val);
        i++;
    }
    return (n += sprintf(&buff[n], "}"));
}

int mkdir(const char *s, mode_t m)
{
    return 0;
}

// EOF