// PicoWi WiFi join functions, see http://iosoft.blog/picowi for details
//
// Copyright (c) 2022, Jeremy P Bentham
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
#include <string.h>

#include "picowi_defs.h"
#include "picowi_wifi.h"
#include "picowi_pico.h"
#include "picowi_init.h"
#include "picowi_ioctl.h"
#include "picowi_regs.h"
#include "picowi_event.h"
#include "picowi_evtnum.h"
#include "picowi_join.h"
#include "picowi_auth.h"

const char country_data[20] = COUNTRY "\x00\x00\xFF\xFF\xFF\xFF" COUNTRY "\x00\x00";
const uint8_t mcast_addr[10 * 6] = { 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x5E, 0x00, 0x00, 0xFB };
const EVT_STR join_evts[]={EVT(WLC_E_JOIN), EVT(WLC_E_ASSOC), EVT(WLC_E_REASSOC), 
    EVT(WLC_E_ASSOC_REQ_IE), EVT(WLC_E_ASSOC_RESP_IE), EVT(WLC_E_SET_SSID),
    EVT(WLC_E_LINK), EVT(WLC_E_AUTH), EVT(WLC_E_PSK_SUP),  EVT(WLC_E_EAPOL_MSG),
    EVT(WLC_E_DISASSOC_IND), EVT(WLC_E_CSA_COMPLETE_IND),
    EVT(-1)};

extern EVENT_INFO event_info;

#define EAPOL_TIMEOUT       2500

// Pre Shared Key authentication
#define WSEC_MIN_PSK_LEN    8
#define WSEC_MAX_PSK_LEN    64
#pragma pack(1)
typedef struct
{
    uint16_t key_len;
    uint16_t flags;
    uint8_t key[WSEC_MAX_PSK_LEN + 1];
} WSEC_PSK;

// Advanced Encryption Standard authentication
#define WSEC_MAX_AES_PWD_LEN 128
typedef struct
{
    uint16_t password_len;
    uint8_t password[WSEC_MAX_AES_PWD_LEN];
} WSEC_AES_PASSWD;
#pragma pack()

// Type of authentication currently in use
uint32_t auth_type;

// Start to join a network
bool join_start(uint32_t auth, char *ssid, char *passwd)
{
    uint32_t val;
    bool ret;
    
    join_stop();
    // Clear data unavail error
    val = wifi_reg_read(SD_FUNC_BUS, BUS_INTEN_REG, 2);
    if (val & 1)
        wifi_reg_write(SD_FUNC_BUS, BUS_INTEN_REG, val, 2);
    // Set sleep KSO (should poll to check for success)
    wifi_reg_write(SD_FUNC_BAK, BAK_SLEEP_CSR_REG, 1, 1);
    wifi_reg_write(SD_FUNC_BAK, BAK_SLEEP_CSR_REG, 1, 1);
    val = wifi_reg_read(SD_FUNC_BAK, BAK_SLEEP_CSR_REG, 1);
    // Select antenna 
    //ret = ret && ioctl_wr_int32(WLC_SET_ANTDIV, IOCTL_WAIT, 0x00) > 0;
    // Data aggregation
    ret = ioctl_set_uint32("bus:txglom", IOCTL_WAIT, 0x00) > 0;
    if (ioctl_set_uint32("apsta", IOCTL_WAIT, 0x01) < 0)
        display(DISP_IOCTL, "IOCTL: APSTA not supported\n");
    ret = ret && ioctl_set_uint32("ampdu_ba_wsize", IOCTL_WAIT, 0x08) > 0;
    ret = ret && ioctl_set_uint32("ampdu_mpdu", IOCTL_WAIT, 0x04) > 0;
    ioctl_set_uint32("ampdu_rx_factor", IOCTL_WAIT, 0x00);
    // Set country
    ret = ret && ioctl_set_data2("country", 8, IOCTL_WAIT, (void *)country_data, sizeof(country_data)) > 0;
    ret = ret && ioctl_wr_int32(WLC_SET_GMODE, IOCTL_WAIT, 0x01) > 0;
    ioctl_err_display(ret);
    //usdelay(100000);
    events_enable(join_evts);
    return(ret);
}

// Stop trying to join network
// (Set WiFi interface 'down', ignore IOCTL response)
bool join_stop(void)
{
    ioctl_wr_data(WLC_DISASSOC, 50, 0, 0);
    ioctl_wr_data(WLC_DOWN, 50, 0, 0); 
    return(true);
}

// Retry joining a network
bool join_restart(uint32_t auth, char *ssid, char *passwd)
{
    uint32_t n;
    uint8_t data[100];
    bool ret = 0;
    
    // Start up the interface
    ioctl_wr_data(WLC_UP, 500, 0, 0);
    if (ioctl_get_data("ver", 10, data, sizeof(data)))
        display(DISP_INFO, "WiFi %s", data);
    ret = ioctl_set_uint32("pm2_sleep_ret", IOCTL_WAIT, 0xc8) > 0 &&
          ioctl_set_uint32("bcn_li_bcn", IOCTL_WAIT, 0x01) > 0 &&
          ioctl_set_uint32("bcn_li_dtim", IOCTL_WAIT, 0x01) > 0 &&
          ioctl_set_uint32("assoc_listen", IOCTL_WAIT, 0x0a) > 0;
#if POWERSAVE    
    // Enable power saving
    init_powersave();
#endif    
    ret = join_security(auth, passwd, strlen(passwd));
    n = strlen(ssid);
    *(uint32_t *)data = n;
    strcpy((char *)&data[4], ssid);
    ret = ret && ioctl_wr_data(WLC_SET_SSID, IOCTL_WAIT, data, n + 4) > 0;
    ioctl_err_display(ret);
    return (ret);
}

// Set WiFi security; derived from whd_wifi_prepare_join in whd_wifi_api.c
bool join_security(uint32_t auth, char *key, int keylen)
{
    bool ret;
    uint32_t dat[2] = { 0, 0 };
    WSEC_PSK psk;
    WSEC_AES_PASSWD aes;
    
    auth_type = auth;
    ret = ioctl_wr_int32(WLC_SET_WSEC, IOCTL_WAIT, auth & 0xff) > 0 &&
    ioctl_set_uint32("roam_off", IOCTL_WAIT, 0);
    dat[1] = (auth & WPA_SECURITY) || (auth & WPA2_SECURITY) || (auth & WPA3_SECURITY) ? 1 : 0;
    ret = ret && ioctl_set_data("bsscfg:sup_wpa", IOCTL_WAIT, dat, 8) > 0;
    dat[0] = 0;
    dat[1] = 0xffffffff;
    ret = ret && ioctl_set_data("bsscfg:sup_wpa2_eapver", IOCTL_WAIT, dat, 8) > 0;
    if (auth == WHD_SECURITY_WPA_TKIP_PSK  ||       auth == WHD_SECURITY_WPA_AES_PSK  ||
        auth == WHD_SECURITY_WPA_MIXED_PSK ||       auth == WHD_SECURITY_WPA2_AES_PSK ||
        auth == WHD_SECURITY_WPA2_AES_PSK_SHA256 || auth == WHD_SECURITY_WPA2_TKIP_PSK ||
        auth == WHD_SECURITY_WPA2_MIXED_PSK ||      auth == WHD_SECURITY_WPA2_WPA_AES_PSK ||
        auth == WHD_SECURITY_WPA2_WPA_MIXED_PSK)
    {
        dat[0] = 0;
        dat[1] = EAPOL_TIMEOUT;
        ret = ret && ioctl_set_data("bsscfg:sup_wpa_tmo", IOCTL_WAIT, dat, 8);
        memset(&psk, 0, sizeof(psk));
        psk.key_len = keylen;
        psk.flags = 1;
        memcpy((char *)psk.key, key, keylen);
        //usdelay(1000);
        ret = ret && ioctl_wr_data(WLC_SET_WSEC_PMK, IOCTL_WAIT, &psk, sizeof(psk)) > 0;
    }
    if (auth == WHD_SECURITY_WPA3_AES || auth == WHD_SECURITY_WPA3_WPA2_PSK)
    {
        dat[0] = 0;
        dat[1] = 2500;
        ret = ret && ioctl_set_data("bsscfg:sup_wpa_tmo", IOCTL_WAIT, dat, 8);
        memset(&aes, 0, sizeof(aes));
        aes.password_len = keylen;
        memcpy(aes.password, key, keylen);
        //usdelay(1000);
        ret = ret && ioctl_set_data("sae_password", IOCTL_WAIT, &aes, sizeof(aes));   
    }
    ret = ret && ioctl_wr_int32(WLC_SET_INFRA, 50, auth & IBSS_ENABLED ? 0 : 1) > 0;
    uint32_t a = (auth == WHD_SECURITY_WPA3_AES) || (auth == WHD_SECURITY_WPA3_WPA2_PSK) ? 3 : 0;        
    ret = ret && ioctl_wr_int32(WLC_SET_AUTH, IOCTL_WAIT, a) > 0;
    uint32_t mfp = auth == WHD_SECURITY_WPA3_AES ? 2 : auth == WHD_SECURITY_WPA3_WPA2_PSK || auth & WPA2_SECURITY ? 1 : 0;
    ret = ret && ioctl_set_uint32("mfp", IOCTL_WAIT, mfp);
    uint32_t wpa = auth == WHD_SECURITY_OPEN || auth == WHD_SECURITY_WPS_SECURE ? 0 :
        auth == WHD_SECURITY_WPA_TKIP_PSK || auth == WHD_SECURITY_WPA_AES_PSK || auth == WHD_SECURITY_WPA_MIXED_PSK ? 4 :
        auth == WHD_SECURITY_WPA2_AES_PSK || auth == WHD_SECURITY_WPA2_TKIP_PSK || auth == WHD_SECURITY_WPA2_MIXED_PSK ||    
            auth == WHD_SECURITY_WPA2_WPA_AES_PSK || auth == WHD_SECURITY_WPA2_WPA_MIXED_PSK ? 0x80 :
        auth == WHD_SECURITY_WPA2_AES_PSK_SHA256 ? 0x8000 :
        auth == WHD_SECURITY_WPA3_AES || auth == WHD_SECURITY_WPA3_WPA2_PSK ? 0x40000 : 
        auth == WHD_SECURITY_WPA_TKIP_ENT || auth == WHD_SECURITY_WPA_AES_ENT || auth == WHD_SECURITY_WPA_MIXED_ENT ? 2 :
        auth == WHD_SECURITY_WPA2_TKIP_ENT || auth == WHD_SECURITY_WPA2_AES_ENT || auth == WHD_SECURITY_WPA2_MIXED_ENT ?  0x40 : 0;
    ret = ret && ioctl_wr_int32(WLC_SET_WPA_AUTH, IOCTL_WAIT, wpa);
    return (ret);
}

// Handler for join events (link & auth changes)
int join_event_handler(EVENT_INFO *eip)
{
    int ret = 1;
    uint16_t news;
    
    if (eip->chan == SDPCM_CHAN_EVT)
    {
        news = eip->link;
        if (auth_type == WHD_SECURITY_OPEN)
            news |= LINK_AUTH_OK;        
        if (eip->event_type==WLC_E_LINK && eip->status==0)
            news = eip->flags&1 ? news|LINK_UP_OK : news&~LINK_UP_OK;
        else if (eip->event_type == WLC_E_PSK_SUP)
            news = eip->status==6 ? news|LINK_AUTH_OK : news&~LINK_AUTH_OK;
        else if (eip->event_type == WLC_E_DISASSOC_IND)
            news = LINK_FAIL;
        else if (eip->event_type == WLC_E_SET_SSID)
        {
            if (eip->status == 1)
                display(DISP_INFO, "Can't join network, check security settings\n");
            else if (eip->status == 3)
                display(DISP_INFO, "Can't find network\n");
        }
        else
            ret = 0;
        eip->link = news;
    }
    else
        ret = 0;
    return(ret);
}

// Poll the network joining state machine
void join_state_poll(uint32_t auth, char *ssid, char *passwd)
{
    EVENT_INFO *eip = &event_info;
    static uint32_t join_ticks;
    static char *s = "", *p = "";

    if (ssid)
        s = ssid;
    if (passwd)
        p = passwd;
    if (eip->join == JOIN_IDLE)
    {
        display(DISP_JOIN, "Joining network %s\n", s);
        eip->link = 0;
        eip->join = JOIN_JOINING;
        ustimeout(&join_ticks, 0);
        join_restart(auth, s, p);
    }
    else if (eip->join == JOIN_JOINING)
    {
        if (link_check() > 0)
        {
            display(DISP_JOIN, "Joined network\n");
            eip->join = JOIN_OK;
        }
        else if (link_check()<0 || ustimeout(&join_ticks, JOIN_TRY_USEC))
        {
            display(DISP_JOIN, "Failed to join network\n");
            ustimeout(&join_ticks, 0);
            join_stop();
            eip->join = JOIN_FAIL;
        }
    }
    else if (eip->join == JOIN_OK)
    {
        ustimeout(&join_ticks, 0);
        if (link_check() < 1)
        {
            display(DISP_JOIN, "Leaving network\n");
            join_stop();
            eip->join = JOIN_FAIL;
        }
    }
    else  // JOIN_FAIL
    {
        if (ustimeout(&join_ticks, JOIN_RETRY_USEC))
            eip->join = JOIN_IDLE;
    }
}

// Return 1 if joined to network, -1 if error joining
int link_check(void)
{
    EVENT_INFO *eip=&event_info;
    return((eip->link&LINK_OK) == LINK_OK ? 1 : 
            eip->link == LINK_FAIL ? -1 : 0);
}

// EOF
