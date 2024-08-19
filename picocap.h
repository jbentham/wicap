// Definitions for Pico data capture

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

// Pin definitions
#define PIN_TEST        16
#define PIN_PCNT        17
#define PIN_SER_TX      20
#define PIN_SER_RX      21
#define PIN_SCK         22
#define PIN_DAC_CS      27
#define PIN_LED         28

// Data input
#define PIN_DIN0        0
#define NUM_DINS        16

// Serial console
#define UART_HW         uart1
#define UART_BAUD       115200

// Master clock frequency
#define PWM_CLOCK       120000000

#define IP_VAL(a,b,c,d) (a<<24|b<<16|c<<8|d)
#define XSAMP_MAX       100000      // Max number of samples
#define XSAMP_PRE       20          // Number of pre-samples to be discarded
#define XSAMP_DEFAULT   1000        // Default number of samples
#define XRATE_DEFAULT   10000       // Default sample rate
//#define IP_BASE_DEFAULT IP_VAL(192, 168, 9, 10) // Default IP base addr
//#define GATEWAY_DEFAULT IP_VAL(192, 168, 9, 1)  // Default gateway addr
#define IP_BASE_DEFAULT 0           // Default IP base addr
#define GATEWAY_DEFAULT 0           // Default gateway addr

#define TEMPS_SIZE      2000        // Size of temporary string buffer

#define NUM_STATES  3
typedef enum { STATE_IDLE, STATE_READY, STATE_CAPTURING, STATE_ERROR} STATE_VALS;
#define STATE_STRS "Idle", "Ready", "Capturing"

#define PARAM_MAXNAME       15      // Maximum length of parameter name
#define PARAM_MAXSTR        31      // Maximum length of parameter string value
typedef struct {
    char name[PARAM_MAXNAME + 1];
    int type;
    union
    {
        uint val;
        char str[PARAM_MAXSTR+1];
    };
} SERVER_PARAM;

typedef enum {ARG_STATUS_T = 1, ARG_CMD_T, ARG_VAL_T, ARG_STR_T, ARG_IP_T} PARAM_TYPES;
typedef enum {
    ARG_STATE, ARG_NSAMP, ARG_CMD, 
    ARG_XSAMP, ARG_XRATE,
    ARG_SECURITY, ARG_SSID, ARG_PASSWD,
    ARG_UNIT, ARG_IP_BASE, ARG_GATEWAY, ARG_END 
} SERVER_ARG_NUM;

#define SERVER_PARAM_VALS                           \
/* Current state */                                 \
    { "state",    ARG_STATUS_T, .val=STATE_IDLE},   \
    { "nsamp",    ARG_STATUS_T, .val=0},            \
/* Commands */                                      \
    { "cmd",      ARG_CMD_T,    .val=0},            \
/* Current configuration */                         \
    { "xsamp",    ARG_VAL_T,    .val=XSAMP_DEFAULT},\
    { "xrate",    ARG_VAL_T,    .val=XRATE_DEFAULT},\
/* Network */                                       \
    { "security", ARG_STR_T,    .val=0},            \
    { "ssid",     ARG_STR_T,    .val=0},            \
    { "passwd",   ARG_STR_T,    .val=0},            \
    { "unit",     ARG_VAL_T,    .val=1},            \
    { "ip_base",  ARG_IP_T,     .val=IP_BASE_DEFAULT},\
    { "gateway",  ARG_IP_T,     .val=GATEWAY_DEFAULT},\
/* End-marker */                                    \
    { "",         0,            .val=0}

#define NUM_CMDS    3
typedef enum { CMD_STOP, CMD_SINGLE, CMD_MULTI } CMD_VALS;

void cap_init(void);
void cap_pio_init(void);
void cap_pout_init(int pin, int freq);
void cap_pout_freq(int pin, int freq);
void cap_start(void *destp, int nsamp);
bool cap_capturing(void);
void cap_set_state(STATE_VALS val);
void cap_end(void);
void cap_set_led(bool on); 
bool mstimeout(uint *tickp, uint msec);
int get_param_int(SERVER_ARG_NUM n);
void set_param_int(SERVER_ARG_NUM n, int val);
uint flash_size(void);
uint flash_sector_size(void);
void flash_sector_write(uint oset, void *data, int dlen);

// EOF
