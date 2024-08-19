// PicoWi WiFi join definitions, see http://iosoft.blog/picowi for details
/*
 * Copyright 2023, Cypress Semiconductor Corporation (an Infineon company)
 * SPDX-License-Identifier: Apache-2.0
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Enable power-saving (increases network response time)
#define POWERSAVE           0

// Flags for EVENT_INFO link state
#define LINK_UP_OK          0x01
#define LINK_AUTH_OK        0x02
#define LINK_OK            (LINK_UP_OK+LINK_AUTH_OK)
#define LINK_FAIL           0x04

#define JOIN_IDLE           0
#define JOIN_JOINING        1
#define JOIN_OK             2
#define JOIN_FAIL           3

#define JOIN_TRY_USEC       10000000
#define JOIN_RETRY_USEC     10000000

bool join_start(uint32_t auth, char *ssid, char *passwd);
bool join_stop(void);
bool join_restart(uint32_t auth, char *ssid, char *passwd);
bool join_security(uint32_t auth, char *key, int keylen);
int join_event_handler(EVENT_INFO *eip);
void join_state_poll(uint32_t auth, char *ssid, char *passwd);
int link_check(void);
int ip_event_handler(EVENT_INFO *eip);

// EOF