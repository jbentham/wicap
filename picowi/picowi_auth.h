// PicoWi authentication definitions, see http://iosoft.blog/picowi for details
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

#define WEP_ENABLED             0x0001  /**< Flag to enable WEP Security        */
#define TKIP_ENABLED            0x0002  /**< Flag to enable TKIP Encryption     */
#define AES_ENABLED             0x0004  /**< Flag to enable AES Encryption      */
#define SHARED_ENABLED      0x00008000  /**< Flag to enable Shared key Security */
#define WPA_SECURITY        0x00200000  /**< Flag to enable WPA Security        */
#define WPA2_SECURITY       0x00400000  /**< Flag to enable WPA2 Security       */
#define WPA2_SHA256_SECURITY 0x0800000 /**< Flag to enable WPA2 SHA256 Security */
#define WPA3_SECURITY       0x01000000  /**< Flag to enable WPA3 PSK Security   */
#define SECURITY_MASK      (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED) /**< Flag to Security mask */

#define ENTERPRISE_ENABLED  0x02000000  /**< Flag to enable Enterprise Security */
#define WPS_ENABLED         0x10000000  /**< Flag to enable WPS Security        */
#define IBSS_ENABLED        0x20000000  /**< Flag to enable IBSS mode           */
#define FBT_ENABLED         0x40000000  /**< Flag to enable FBT                 */

typedef enum
{
WHD_SECURITY_OPEN             = 0, /**< Open security                                         */
WHD_SECURITY_WEP_PSK          = WEP_ENABLED, /**< WEP PSK Security with open authentication             */
WHD_SECURITY_WEP_SHARED       = (WEP_ENABLED | SHARED_ENABLED), /**< WEP PSK Security with shared authentication           */
WHD_SECURITY_WPA_TKIP_PSK     = (WPA_SECURITY | TKIP_ENABLED), /**< WPA PSK Security with TKIP                            */
WHD_SECURITY_WPA_AES_PSK      = (WPA_SECURITY | AES_ENABLED), /**< WPA PSK Security with AES                             */
WHD_SECURITY_WPA_MIXED_PSK    = (WPA_SECURITY | AES_ENABLED | TKIP_ENABLED), /**< WPA PSK Security with AES & TKIP                      */
WHD_SECURITY_WPA2_AES_PSK     = (WPA2_SECURITY | AES_ENABLED), /**< WPA2 PSK Security with AES                            */
WHD_SECURITY_WPA2_AES_PSK_SHA256 = (WPA2_SECURITY | WPA2_SHA256_SECURITY | AES_ENABLED), /**< WPA2 PSK SHA256 Security with AES                     */
WHD_SECURITY_WPA2_TKIP_PSK    = (WPA2_SECURITY | TKIP_ENABLED), /**< WPA2 PSK Security with TKIP                           */
WHD_SECURITY_WPA2_MIXED_PSK   = (WPA2_SECURITY | AES_ENABLED | TKIP_ENABLED), /**< WPA2 PSK Security with AES & TKIP                     */
WHD_SECURITY_WPA2_FBT_PSK     = (WPA2_SECURITY | AES_ENABLED | FBT_ENABLED), /**< WPA2 FBT PSK Security with AES & TKIP */
WHD_SECURITY_WPA3_AES         = (WPA3_SECURITY | AES_ENABLED), /**< WPA3 Security with AES */
WHD_SECURITY_WPA2_WPA_AES_PSK = (WPA2_SECURITY | WPA_SECURITY | AES_ENABLED), /**< WPA2 WPA PSK Security with AES                        */
WHD_SECURITY_WPA2_WPA_MIXED_PSK = (WPA2_SECURITY | WPA_SECURITY | AES_ENABLED | TKIP_ENABLED), /**< WPA2 WPA PSK Security with AES & TKIP                 */
WHD_SECURITY_WPA3_WPA2_PSK    = (WPA3_SECURITY | WPA2_SECURITY | AES_ENABLED), /**< WPA3 WPA2 PSK Security with AES */
WHD_SECURITY_WPA_TKIP_ENT     = (ENTERPRISE_ENABLED | WPA_SECURITY | TKIP_ENABLED), /**< WPA Enterprise Security with TKIP                     */
WHD_SECURITY_WPA_AES_ENT      = (ENTERPRISE_ENABLED | WPA_SECURITY | AES_ENABLED), /**< WPA Enterprise Security with AES                      */
WHD_SECURITY_WPA_MIXED_ENT    = (ENTERPRISE_ENABLED | WPA_SECURITY | AES_ENABLED | TKIP_ENABLED), /**< WPA Enterprise Security with AES & TKIP               */
WHD_SECURITY_WPA2_TKIP_ENT    = (ENTERPRISE_ENABLED | WPA2_SECURITY | TKIP_ENABLED), /**< WPA2 Enterprise Security with TKIP                    */
WHD_SECURITY_WPA2_AES_ENT     = (ENTERPRISE_ENABLED | WPA2_SECURITY | AES_ENABLED), /**< WPA2 Enterprise Security with AES                     */
WHD_SECURITY_WPA2_MIXED_ENT   = (ENTERPRISE_ENABLED | WPA2_SECURITY | AES_ENABLED | TKIP_ENABLED), /**< WPA2 Enterprise Security with AES & TKIP              */
WHD_SECURITY_WPA2_FBT_ENT     = (ENTERPRISE_ENABLED | WPA2_SECURITY | AES_ENABLED | FBT_ENABLED), /**< WPA2 Enterprise Security with AES & FBT               */
WHD_SECURITY_IBSS_OPEN        = (IBSS_ENABLED), /**< Open security on IBSS ad-hoc network                  */
WHD_SECURITY_WPS_SECURE       = AES_ENABLED, /**< WPS with AES security                                 */
} whd_security_t;

// EOF