// Pico data capture

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

#include <stdio.h>
#include <stdbool.h>

#include <hardware/pwm.h>
#include <hardware/dma.h>
#include <hardware/pio.h>
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "picowi/picowi_defs.h"
#include "pico/stdlib.h"
#include "picocap.h"
#include "picocap.pio.h"

static PIO cap_pio = pio1;
static uint cap_sm;

const char *state_strs[NUM_STATES] = { STATE_STRS };

SERVER_PARAM server_params[] = { SERVER_PARAM_VALS };

WORD samples[XSAMP_MAX+XSAMP_PRE];

uint pout_slice, cap_dma_chan;

// Storage of configuration in Flash memory
#define CONFIG_FLASH_SIZE   FLASH_SECTOR_SIZE
uint config_flash_oset;

// Initialise capture pins
void cap_init(void)
{
    int n;

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, 1);
    for (n=0; n<NUM_DINS; n++)
    {
        gpio_init(n);
        gpio_set_dir(n, 0);
    }
    gpio_init(PIN_TEST);
    gpio_set_dir(PIN_TEST, 1);
    cap_pio_init();
    config_flash_oset = flash_size() - CONFIG_FLASH_SIZE;
}

// Initialise PIO for data capture
void cap_pio_init(void)
{
    cap_sm = pio_claim_unused_sm(cap_pio, true);
    uint offset = pio_add_program(cap_pio, &picocap_program);
    pio_sm_config c = picocap_program_get_default_config(offset);
    pio_sm_set_consecutive_pindirs(cap_pio, cap_sm, PIN_DIN0, NUM_DINS, false);
    pio_gpio_init(cap_pio, PIN_TEST);
    pio_sm_set_consecutive_pindirs(cap_pio, cap_sm, PIN_TEST, 1, true);
    sm_config_set_sideset_pins(&c, PIN_TEST);
    sm_config_set_in_pins(&c, PIN_DIN0);
    sm_config_set_clkdiv(&c, 1);
    sm_config_set_in_shift(&c, false, true, 16);
    pio_sm_clear_fifos(cap_pio, cap_sm);
    pio_sm_init(cap_pio, cap_sm, offset, &c);
    pio_sm_set_enabled(cap_pio, cap_sm, true);
}


// Initialise capture clock pulse output
void cap_pout_init(int pin, int freq) 
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    pout_slice = pwm_gpio_to_slice_num(pin);
    cap_pout_freq(pin, freq);

    cap_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(cap_dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, pwm_get_dreq(pout_slice));
    dma_channel_configure(cap_dma_chan, &cfg, NULL, &cap_pio->rxf[cap_sm], 0, false);
}

// Set capture frequency
void cap_pout_freq(int pin, int freq)
{
    int wrap = (PWM_CLOCK / freq) & 0xffff;

    pwm_set_enabled(pout_slice, false);
    pwm_set_clkdiv_int_frac(pout_slice, 1, 0);
    pwm_set_wrap(pout_slice, wrap - 1);
    pwm_set_chan_level(pout_slice, pwm_gpio_to_channel(pin), wrap / 2);
    pwm_set_phase_correct(pout_slice, 0);
}

// Start a capture
void cap_start(void *destp, int nsamp)
{
    cap_pout_freq(PIN_SCK, get_param_int(ARG_XRATE));
    pwm_set_counter(pout_slice, 0);
    pwm_set_enabled(pout_slice, true);
    dma_channel_abort(cap_dma_chan);
    dma_channel_set_write_addr(cap_dma_chan, destp, false);
    dma_channel_set_trans_count(cap_dma_chan, nsamp, true);
}

// Check progress of capture
bool cap_capturing(void)
{
    int rem = dma_channel_hw_addr(cap_dma_chan)->transfer_count;
    int xsamp = get_param_int(ARG_XSAMP);
    int nsamp = xsamp - rem;

    set_param_int(ARG_NSAMP, nsamp<0 ? 0 : nsamp);
    return (rem > 0);
}

// Set the current state
void cap_set_state(STATE_VALS val) 
{
    set_param_int(ARG_STATE, val);
    if (val < NUM_STATES)
        printf("State: %s\r\n", state_strs[val]);
}

// End a capture
void cap_end(void)
{
    dma_channel_abort(cap_dma_chan);
    pwm_set_enabled(pout_slice, false);
}

// Set or clear the capture LED
void cap_set_led(bool on) 
{
    gpio_put(PIN_LED, !on);
}

// Get integer parameter value, given index number
int get_param_int(SERVER_ARG_NUM n)
{
    return (n <= ARG_END) ? server_params[n].val : 0;
}
// Set integer parameter value, given index number
void set_param_int(SERVER_ARG_NUM n, int val)
{
    if (n < ARG_END)
        server_params[n].val = val;
}

// Return size of flash memory
uint flash_size(void) 
{
    uint8_t txbuf[4] = {0x9f, 0, 0, 0}, rxbuf[4] = {0, 0, 0, 0};
    uint stat = save_and_disable_interrupts();

    flash_do_cmd(txbuf, rxbuf, sizeof(txbuf));
    restore_interrupts(stat);
    return (1 << rxbuf[3]);
}

// Erase & write a single flash sector, length must be mutiple of 256
void flash_sector_write(uint oset, void *data, int dlen)
{
    uint stat = save_and_disable_interrupts();

    flash_range_erase(oset, FLASH_SECTOR_SIZE);
    flash_range_program(oset, data, dlen);
    restore_interrupts(stat);
}

// EOF
