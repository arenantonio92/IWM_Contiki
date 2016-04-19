#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-debug.h"

#include "rest-engine.h"


#include "simple-udp.h"
#include "servreg-hack.h"

#include "net/rpl/rpl.h"

#include <stdio.h>
#include <string.h>

/* SERVREG SERVICE_ID  */
#define SERVICE_ID 190

extern resource_t proxy_resource;

/*---------------------------------------------------------------------------*/
PROCESS(proxy_server_process, "Proxy Server process");
AUTOSTART_PROCESSES(&proxy_server_process);
/*---------------------------------------------------------------------------*/

static uip_ipaddr_t *set_global_address(void)
{
  static uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  printf("First part addr: ");
  uip_debug_ipaddr_print(&ipaddr);
  printf("\n");
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
  printf("Final addr: ");
  uip_debug_ipaddr_print(&ipaddr);
  printf("\n");

  printf("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }

  return &ipaddr;
}

static void create_rpl_dag(uip_ipaddr_t *ipaddr)
{
  struct uip_ds6_addr *root_if;

  root_if = uip_ds6_addr_lookup(ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    uip_ipaddr_t prefix;
    
    rpl_set_root(RPL_DEFAULT_INSTANCE, ipaddr);
    dag = rpl_get_any_dag();
    uip_ip6addr(&prefix, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &prefix, 64);
    PRINTF("created a new RPL dag\n");
  } else {
    PRINTF("failed to create a new RPL DAG\n");
  }
}

/* --------------------------------------------------------- */
PROCESS_THREAD(proxy_server_process, ev, data)
{
	uip_ipaddr_t *ipaddr;
	
	PROCESS_BEGIN();
	
	servreg_hack_init();
	
	ipaddr = set_global_address();

	//create_rpl_dag(ipaddr);
	
	servreg_hack_register(SERVICE_ID, ipaddr);
	
	rest_init_engine();
	
	rest_activate_resource(&proxy_resource, "proxy_resource");


	while(1){
		PROCESS_WAIT_EVENT();
	}
	
	PROCESS_END();
}
