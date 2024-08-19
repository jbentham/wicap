// Definitions for Mongoose picowi WiFi driver

#define SSID_MAXLEN     32
#define PASSWD_MAXLEN   63

// printf with Mongoose extensions
// First mg_xprintf arg is an output function, 2nd is arg to that function
#define xprintf(...) mg_xprintf(&mg_pfn_stdout, 0, __VA_ARGS__)
#define xprint_ip(ip) mg_print_ip4(&mg_pfn_stdout, 0, ip)

int wifi_poll(void *buf, size_t buflen);
void join_timer_fn(void *arg);
extern struct mg_tcpip_driver mg_tcpip_driver_wifi;
char *auth_type_str(int typ);
int auth_str_type(char *s);
void ip_addr_str(uint ip, char *buff);

// EOF
