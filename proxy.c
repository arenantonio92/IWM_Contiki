#include "contiki.h"
#include "lib/random.h"

#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-debug.h"

#include "rest-engine.h"

#include "servreg-hack.h"

#include <stdio.h>
#include <string.h>

/* SERVREG SERVICE_ID  */
#define SERVICE_ID 			190

#define RANDOM_MAX		 	65535
#define A_CODE				65
#define Z_CODE				90

extern resource_t proxy_res;
char proxy_name;

/*---------------------------------------------------------------------------*/
PROCESS(proxy_server_process, "Proxy Server process");
AUTOSTART_PROCESSES(&proxy_server_process);
/*---------------------------------------------------------------------------*/

unsigned int
randr(unsigned int min, unsigned int max)
{
       double scaled = (double)random_rand()/RANDOM_MAX;

       return (max - min)*scaled + min;
}

static uip_ipaddr_t *set_global_address(void)
{
  static uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  printf("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }
  
  printf("initializing rand with seed: %u\n",(unsigned short)ipaddr.u16[7]);
  random_init((unsigned short)ipaddr.u16[7]);
  return &ipaddr;
}


/* --------------------------------------------------------- */
PROCESS_THREAD(proxy_server_process, ev, data)
{
	uip_ipaddr_t *my_ip;
	
	PROCESS_BEGIN();
	
	my_ip = set_global_address();
	
	unsigned int num = randr(A_CODE, Z_CODE);
	
	proxy_name = (char)num;
	printf("Hi, I'm the proxy %c\n", proxy_name);
	
	rest_init_engine();
	
	printf("Activating proxy resource\n");
	rest_activate_resource(&proxy_res, "proxy_resource");
	
	servreg_hack_register(SERVICE_ID, my_ip);

	while(1){
		PROCESS_WAIT_EVENT();
	}
	
	PROCESS_END();
}
