#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "contiki.h"
#include "contiki-net.h"
#include "rest-engine.h"
#include "lib/random.h"
#include "dev/button-sensor.h"

#include "servreg-hack.h"
#include "sys/etimer.h"
#include "lib/random.h"
#include "er-coap-engine.h"
#include "uip-debug.h"

#define MAX_LAT				100
#define	MIN_LAT				1
#define MAX_LON				100	
#define MIN_LON				1
#define RANDOM_MAX			65535
#define MAX_AGE 			600
#define SERVICE_ID      	190
#define REMOTE_PORT     	UIP_HTONS(COAP_DEFAULT_PORT)
#define POST_INTERVAL 	    (10 * CLOCK_SECOND)

#define DEBUG DEBUG_FULL

static double vol = 0;
static double aux=0;
static char ip[40];
static double lat;
static double lon;
static uip_ipaddr_t *proxy_ipaddr;
static uip_ipaddr_t *ipaddr;

struct etimer et_do_dht;

void res_per_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void per_handler();

PERIODIC_RESOURCE(resource_per, "title=\"Resource Periodic\";rt=\"Text\"", res_per_get_handler, NULL, NULL, NULL, 500*CLOCK_SECOND, per_handler);


void res_per_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){

	/*stringa volume "{\"e\":{\"v\":\"%03.02f\",\"u\":\"%\"}}"*/
	char jstring[60];
	int length = 40;
	
	int i_part = (int)vol;
	long int f_part = (long int)10000000*(vol-i_part);
	
	printf("GET Handler: ");
	sprintf(jstring, "{'e':{'v':'%d.%ld','u':'%'}}", i_part, f_part);
	printf("%s\n",jstring);
	
	length = strlen(jstring);
			
	memcpy(buffer, jstring, length);

	REST.set_header_content_type(response, REST.type.APPLICATION_JSON); 
	REST.set_header_max_age(response, MAX_AGE);
	REST.set_header_etag(response, (uint8_t *) &length, 1);
	REST.set_response_payload(response, buffer, length);
}

static void per_handler()
{
  /*si riempie il cassonetto a random (saturando al 100%) e si invia il dato */
	printf("Per_handler\n");
	int i_part;
	long int f_part; 

	aux = (double)(random_rand())/RANDOM_MAX;
	/* aux=[0,1]*/
	
	if(aux>0.7)
		;
	else 
		if((vol+aux*100) > 100)
			vol=100.0;
	else 
		vol+=aux*100;
	
	i_part = (int)vol;
	f_part = (long int)10000000*(vol-i_part);
	
	printf("Vol: %d.%ld\n", i_part, f_part);

    /* notificare agli observers che il dato si è (eventualmente) aggiornato. */
    REST.notify_subscribers(&resource_per);
  
}
void client_chunk_handler(void *response)
{
  const uint8_t *chunk;

  int len = coap_get_payload(response, &chunk);

  printf("|%.*s", len, (char *)chunk);
  
}


static uip_ipaddr_t* set_global_address(void)
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
  
  return &ipaddr;

}

float
randr(unsigned int min, unsigned int max)
{
       float scaled = (float)random_rand()/RANDOM_MAX;

       return (float)(max - min +1)*scaled + min;
}

void dump_init(){
	/*inizializzo il cassonetto, dandogli ip, lat e lon (statiche)*/
	ipaddr = set_global_address();
	random_init((unsigned short)ipaddr->u16[7]);
	
	uip_debug_ipaddr_sprint(ip,ipaddr);
	lat = randr(MIN_LAT, MAX_LAT);
	lon = randr(MIN_LON, MAX_LON);
}




/*---------------------------------------------------------------------------*/
PROCESS(Dumpster_proc, "Dumpster process");
PROCESS(Post_proc, "Post Process");
AUTOSTART_PROCESSES(&Dumpster_proc);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(Post_proc, ev, data){
	static coap_packet_t request[1];
	
	
	static char id_string[100];
    int lat_i_part; 
	long int lat_f_part; 
    int lon_i_part; 
	long int lon_f_part;
	
	PROCESS_BEGIN();
	
	/*faccio la post al proxy per annunciare il mio arrivo*/
	coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, "proxy_resource/");

    lat_i_part = (int)lat;
	lat_f_part = (long int)10000000*(lat -lat_i_part);
    
    lon_i_part = (int)lon;
	lon_f_part = (long int)10000000*(lon -lon_i_part);
    
    printf("{'Ip':'[%s]', 'Lat'='%d.%ld', 'Lon':'%d.%ld'}\n ", ip, lat_i_part, lat_f_part, lon_i_part, lon_f_part);
	sprintf(id_string, "{'Ip':'[%s]', 'Lat'='%d.%ld', 'Lon':'%d.%ld'} ", ip, lat_i_part, lat_f_part, lon_i_part, lon_f_part);
    coap_set_payload(request, (uint8_t *)id_string, strlen(id_string));
    
    COAP_BLOCKING_REQUEST(proxy_ipaddr, REMOTE_PORT, request, client_chunk_handler);
    
    PROCESS_END();
}
	
	

PROCESS_THREAD(Dumpster_proc, ev, data)
{
	static struct etimer wait_timer;
	servreg_hack_item_t *item;
	unsigned int period;

	unsigned int WAIT_INTERVAL;
	
	PROCESS_BEGIN();

	servreg_hack_init();
	
	/*attivazione bottone(a cui è associato lo svuotamento del cassonetto)*/
	
	
	/*inizializzo il cassonetto (ip, lat, lon)*/
	dump_init();
	
	SENSORS_ACTIVATE(button_sensor);
	rest_init_engine();
	
	coap_init_engine();
	
	//WAIT ALL IS DONE BY PROXY
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
	/* PERIODO RANDOM e attivazione risorsa periodica */
   // period =  randr(50,100)*CLOCK_SECOND;  
	//printf("Period: %u\n", period);
	//periodic_resource_per.period = period;
	rest_activate_resource(&resource_per, "dumpster");
	
	//etimer_set(&et_do_dht, POST_INTERVAL);
	//process_start(&Post_proc, NULL);
  
	while(1) {
		PROCESS_WAIT_EVENT();
		if(ev == sensors_event && data == &button_sensor){
			printf("BUTTON PRESSED\n");
			vol=0;
		}
	
	}
	PROCESS_END();
}

