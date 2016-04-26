#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "contiki.h"
#include "contiki-net.h"
#include "rest-engine.h"
#include "lib/random.h"
#include "servreg-hack.h"
#include "dev/button-sensor.h"
#include "er-coap-engine.h"

//#include "dev/leds.h"

#define RANDOM_MAX		 	65535
#define SERVICE_ID      	190

#define REMOTE_PORT     	UIP_HTONS(COAP_DEFAULT_PORT)

static float vol = 0;
static float aux=0;
static uip_ipaddr_t *proxy_ipaddr;
static float lat;
static float lon;


void res_per_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void per_handler();


PERIODIC_RESOURCE(resource_per, "title=\"Resource Periodic\";rt=\"Text\"", res_per_get_handler, NULL, NULL, NULL, 500*CLOCK_SECOND, per_handler);


void
res_per_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){

	/* Populat the buffer with the response payload*/
	char jstring[50];
	int length = 40;
	
	int i_part = (int)vol;
	long int f_part = (long int)100*(vol-i_part);
//
	sprintf(jstring, "{'e':{'v':'%d.%ld','u':'%'}}", i_part, f_part);
	length = strlen(jstring);
	memcpy(buffer, jstring, length);

	REST.set_header_content_type(response, REST.type.APPLICATION_JSON); 
	REST.set_header_etag(response, (uint8_t *) &length, 1);
	REST.set_response_payload(response, buffer, length);
}

static void
per_handler()
{
  /* Do the update triggered by the event here, e.g., sampling a sensor. */
	int i_part;
	long int f_part; 

	aux = (float)random_rand()/RANDOM_MAX;
	/* aux=[0,1]*/
	
	if(aux>0.1)
		;
	else 
		if((vol+aux*100) > 100)
			vol=100.0;
	else 
		vol+=aux*100;
	
	i_part = (int)vol;
	f_part = (long int)100*(vol-i_part);
	
	//printf("Value: %d\n",per_value);

    /* Notify the registered observers which will trigger the tget_handler to create the response. */
    REST.notify_subscribers(&resource_per);
  
}


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
 
  random_init((unsigned short)ipaddr.u16[7]);
  return &ipaddr;
}

float randr(unsigned int min, unsigned int max)
{
       float scaled = (float)random_rand()/RANDOM_MAX;

       return (max - min +1)*scaled + min;
}


void client_chunk_handler(void *response)
{
  const uint8_t *chunk;

  int len = coap_get_payload(response, &chunk);

  printf("|%.*s", len, (char *)chunk);
  
}

/*---------------------------------------------------------------------------*/
PROCESS(server, "Server process");
AUTOSTART_PROCESSES(&server);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(server, ev, data)
{
  uip_ipaddr_t *ipaddr;
  servreg_hack_item_t *item;
  static struct etimer wait_timer;
  unsigned int WAIT_INTERVAL;
  static char ip[40];
  
  unsigned int period ;
  
  static coap_packet_t request[1];
	
	
	static char id_string[80];
    int lat_i_part; 
	long int lat_f_part; 
    int lon_i_part; 
	long int lon_f_part;
	
  PROCESS_BEGIN();
  
  ipaddr = set_global_address();	
	
	
  servreg_hack_init();
  
  WAIT_INTERVAL = (unsigned int)randr(50,100)*CLOCK_SECOND;
	
	/* GET PROXY ADDR */ 
	item = servreg_hack_list_head();
	
	while(item == NULL){
		etimer_set(&wait_timer, WAIT_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&wait_timer));
		
		item = servreg_hack_list_head();
	}
	
    for(;item != NULL;item = list_item_next(item)) {
		if(servreg_hack_item_id(item) == SERVICE_ID){
			printf("Id %d address ", servreg_hack_item_id(item));
			uip_debug_ipaddr_print(servreg_hack_item_address(item));
			proxy_ipaddr = servreg_hack_item_address(item);
			printf("\n");
			
			//servreg_hack_register(servreg_hack_item_id(item), proxy_ipaddr);
			break;
		}
     }

  period = randr(500,1500)*CLOCK_SECOND;  

  periodic_resource_per.period = period;
  
  SENSORS_ACTIVATE(button_sensor);
  rest_init_engine();
  rest_activate_resource(&resource_per, "dumpster");
	
	lat = randr(1,100);
	lon = randr(1,100);	
	uip_debug_ipaddr_sprint(ip,ipaddr);
	
   /*faccio la post al proxy per annunciare il mio arrivo*/
	coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "proxy_resource/");

    lat_i_part = (int)lat;
	lat_f_part = (long int)10000000*(lat -lat_i_part);
    
    lon_i_part = (int)lon;
	lon_f_part = (long int)10000000*(lon -lon_i_part);
    
    printf("{'Ip':'[%s]', 'Lat':'%d.%ld', 'Lon':'%d.%ld'}\n ", ip, lat_i_part, lat_f_part, lon_i_part, lon_f_part);
	sprintf(id_string, "{'Ip':'[%s]', 'Lat':'%d.%ld', 'Lon':'%d.%ld'} ", ip, lat_i_part, lat_f_part, lon_i_part, lon_f_part);
    coap_set_payload(request, (uint8_t *)id_string, strlen(id_string));
    
    COAP_BLOCKING_REQUEST(proxy_ipaddr, REMOTE_PORT, request, client_chunk_handler);
  
  
  
  while(1) {
    PROCESS_WAIT_EVENT();
    if(ev == sensors_event && data == &button_sensor){
    	//leds_toggle(LEDS_ALL);
    	printf("Button pressed\n");
    	vol = 0;
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
