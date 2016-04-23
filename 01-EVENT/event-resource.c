#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "contiki.h"
#include "contiki-net.h"
#include "rest-engine.h"
#include "lib/random.h"
#include "dev/button-sensor.h"
//#include "dev/leds.h"

#define RANDOM_MAX		 	65535

static int value;
static int per_value;



void res_event_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
void res_event_post_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void event_handler();

void res_per_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void per_handler();

EVENT_RESOURCE(resource_example, "title=\"Resource\";rt=\"Text\"", res_event_get_handler, 				res_event_post_handler, NULL, NULL, event_handler);
PERIODIC_RESOURCE(resource_per, "title=\"Resource Periodic\";rt=\"Text\"", res_per_get_handler, 			NULL, NULL, NULL, 500*CLOCK_SECOND, per_handler);

void
res_event_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){

	/* Populat the buffer with the response payload*/
	char message[20];
	int length = 20;

	sprintf(message, "VALUE:%03u", value);
	length = strlen(message);
	memcpy(buffer, message, length);

	REST.set_header_content_type(response, REST.type.TEXT_PLAIN); 
	REST.set_header_etag(response, (uint8_t *) &length, 1);
	REST.set_response_payload(response, buffer, length);
}

void
res_event_post_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){


  int new_value, len;
  const char *val = NULL;
     
  len=REST.get_post_variable(request, "value", &val);
     
  if( len > 0 ){
     new_value = atoi(val);	
     value = new_value;
     REST.set_response_status(response, REST.status.CREATED);
  } else {
     REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}

static void
event_handler()
{
  /* Do the update triggered by the event here, e.g., sampling a sensor. */
  ++value;

    /* Notify the registered observers which will trigger the tget_handler to create the response. */
    REST.notify_subscribers(&resource_example);
  
}

void
res_per_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){

	/* Populat the buffer with the response payload*/
	char message[35];
	int length = 20;
//
	sprintf(message, "{'e':{'v':'%03u','u':'%'}}", per_value);
	length = strlen(message);
	memcpy(buffer, message, length);

	REST.set_header_content_type(response, REST.type.APPLICATION_JSON); 
	REST.set_header_etag(response, (uint8_t *) &length, 1);
	REST.set_response_payload(response, buffer, length);
}

static void
per_handler()
{
  /* Do the update triggered by the event here, e.g., sampling a sensor. */
  ++per_value;

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

unsigned int
randr(unsigned int min, unsigned int max)
{
       double scaled = (double)random_rand()/RANDOM_MAX;

       return (max - min +1)*scaled + min;
}


/*---------------------------------------------------------------------------*/
PROCESS(server, "Server process");
AUTOSTART_PROCESSES(&server);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(server, ev, data)
{
  uip_ipaddr_t *ipaddr;
	
	PROCESS_BEGIN();

	ipaddr = set_global_address();	

  unsigned int period = randr(500,1500)*CLOCK_SECOND;  

  periodic_resource_per.period = period;
  
  SENSORS_ACTIVATE(button_sensor);
  rest_init_engine();
  rest_activate_resource(&resource_example, "ev");
  rest_activate_resource(&resource_per, "dumpster");
  while(1) {
    PROCESS_WAIT_EVENT();
    if(ev == sensors_event && data == &button_sensor){
    	//leds_toggle(LEDS_ALL);
    	printf("Button pressed\n");
	resource_example.trigger();
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
