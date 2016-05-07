#include "contiki.h"

#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-debug.h"

#include "servreg-hack.h"

#include <stdio.h>
#include "string.h"
#include "stdlib.h"

#include "er-coap-engine.h"
#include "rest-engine.h"

#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"

#define MAX_MOTES 			3
#define MAX_STRING_LEN		30
#define MAX_AGE 			90
#define REMOTE_PORT     	UIP_HTONS(COAP_DEFAULT_PORT)
#define OBS_RESOURCE_URI 	"dumpster" 


/* SERVREG SERVICE_ID  */
#define SERVICE_ID 			190

#define RANDOM_MAX		 	65535
#define A_CODE				65
#define Z_CODE				90
#define WAIT_INTERVAL 		240*CLOCK_SECOND

char proxy_name;

/* --------- PROXYING RESOURCES ------------------------ */

static uint16_t seq = 0;

static uint8_t initialized = 0;

/* List of event observable resources */
LIST(res_list);
LIST(request_list);

struct string{
	struct string *next;
	char str[MAX_STRING_LEN];
};


struct my_float{
	int i_part;
	long int f_part;
};

struct req{
	struct req *next;
	uint16_t ID;
};

struct proxying_res{
	struct proxying_res *next;
	uint16_t ID;
	uint32_t counter;
	struct my_float volume;
	struct my_float latitude;
	struct my_float longitude;
	resource_t res;
	coap_observee_t *obs;	
	struct string *attr;
	struct string *uri;
};

typedef struct proxying_res proxying_res;

uint32_t old_counters[MAX_MOTES];

//Memory management allocator definition for proxy resources

MEMB(memb_res_allocator, proxying_res, MAX_MOTES);
MEMB(request_allocator, struct req, MAX_MOTES);
MEMB(string_allocator, struct string, MAX_MOTES);
MEMB(uri_allocator, struct string, MAX_MOTES);

list_t restful_services_list;



/* --------------------- FUNCTION DECLARATION  --------------------- */

//Proxy resource post_handler
static void res_post_handler(void *request, void *response, 
	uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
	
RESOURCE(proxy_res,
		"title=\"Fake resource\";rt=\"ProxyResource\"",
		NULL,
		res_post_handler,
		res_post_handler,
		NULL);

//Register a new observation
static void 
register_obs(uip_ip6addr_t *addr, proxying_res *p);

//Remove a specific observation relation and resource assiociated
static void remove_observation(const char *addr);

//Handle the response to the observe request
static void
notification_callback(coap_observee_t *obs, void *notification,
                      coap_notification_flag_t flag);
                      
//Copy the new payload in the correct element list
static void update_payload(uip_ip6addr_t *addr, char *payload);

//Initialize a new resource for Java Cliente
static void init_resource(proxying_res *p);

//Event resource handler
static void event_handler();

//Get Handler
static void res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);


/*---------------------------------------------------------------------------*/
PROCESS(proxy_server_process, "Proxy Server process");
AUTOSTART_PROCESSES(&proxy_server_process);
/*---------------------------------------------------------------------------*/


/* --------- MOTES DISCOVERING and REGISTRATION -------- */

		
static void res_post_handler(void *request, void *response, 
	uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	size_t len = 0;
	const uint8_t *payload = NULL;
	const char *ip = NULL;
	const char *i_lat = NULL;
	const char *f_lat = NULL;
	const char *i_lon = NULL;
	const char *f_lon = NULL;
	proxying_res *s;
	uip_ip6addr_t mote_addr;
	int success = 1;
	
	if((len = REST.get_request_payload(request, &payload))){
		
		ip =  strstr((const char*)payload, ":'");
		printf("New mote with ip: ");
		if(uiplib_ip6addrconv(ip+2, &mote_addr)==0)
			success = 0;
		uip_debug_ipaddr_print(&mote_addr);
		printf("\n");
		
		i_lat = strstr(ip+1, ":'");
		f_lat = strstr(i_lat+1, ".");
		
		i_lon = strstr(f_lat+1,":'");
		f_lon = strstr(i_lon+1,".");
			
		for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
		{			
			if(uip_ipaddr_cmp(&mote_addr, &(s->obs->addr))){
				uip_debug_ipaddr_print(&mote_addr);
				REST.set_response_status(response, REST.status.CREATED);
				printf(" already in database\n");
			    return;
			}	
		}
	}
	
	if(success){
		printf("New observation relation with mote %01x\n",seq);	
			
		if(initialized == 0){
			
			memb_init(&memb_res_allocator);
			memb_init(&request_allocator);
			memb_init(&string_allocator);
			memb_init(&uri_allocator);
					
			list_init(request_list);
			list_init(res_list);
	
			initialized = 1;
			
		}
		
		s = (proxying_res*)memb_alloc(&memb_res_allocator);
		if(s == NULL){
			printf("Memb error: no free objects\n");
			success = 0;
		}
		else{
			s->ID 	 	= seq;
			s->counter	= 0;
			s->attr  	= memb_alloc(&string_allocator);
			s->uri   	= memb_alloc(&uri_allocator);
			
			s->latitude.i_part = atoi(i_lat+2);
			s->latitude.f_part = atol(f_lat+1);
			
			s->longitude.i_part = atoi(i_lon+2);
			s->longitude.f_part = atol(f_lon+1);
			
			//printf("lat:%03d.%07ld, long:%03d.%07ld\n", p->latitude.i_part, p->latitude.f_part, p->longitude.i_part, p->longitude.f_part);
			
			init_resource(s);
			register_obs(&mote_addr, s);
			list_add(res_list, s);
			seq++;
			
			REST.set_response_status(response, REST.status.CREATED);
		}
	}
	
	if(!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
	}
}

/* ------------ OBSERVING FUNCTION -------------------- */
static void 
register_obs(uip_ip6addr_t *addr, proxying_res *p)
{
	p->obs = coap_obs_request_registration(addr, REMOTE_PORT,
                        OBS_RESOURCE_URI, notification_callback, NULL);
}

static void init_resource(proxying_res *p)
{
	
	sprintf(p->attr->str, "title=\"%c%02d\";rt=\"Text\"", proxy_name, p->ID);
	
	//printf("Initizalizing Resource for mote %02d\n", p->ID);
	
	p->res.next 			= NULL;
	p->res.url 				= NULL;
	p->res.flags 			= IS_OBSERVABLE;
	p->res.attributes   	= p->attr->str;
	p->res.get_handler  	= res_get_handler;
	p->res.post_handler 	= NULL;
	p->res.put_handler 		= NULL;
	p->res.delete_handler 	= NULL;
	p->res.trigger 			= event_handler;


	sprintf(p->uri->str, "%c%02u", proxy_name, p->ID);
	
	//printf("Activating Resource for mote %02d\n", p->ID);
	rest_activate_resource(&p->res, p->uri->str);
}

static void
notification_callback(coap_observee_t *obs, void *notification,
                      coap_notification_flag_t flag)
{
	printf("Notification handler\n");
	
	int len = 0;
	const uint8_t *payload = NULL;
	char addr[MAX_STRING_LEN];
	
	uip_debug_ipaddr_sprint(addr, &(obs->addr));
	printf("Observee addr: %s\n", addr);
	printf("Observee URI: %s\n", obs->url);
	
		
	if(notification){
		len = coap_get_payload(notification, &payload);
	}
	
	switch(flag) {
	  case NOTIFICATION_OK:
		printf("NOTIFICATION OK: %*s\n", len, (char *)payload);
		update_payload(&obs->addr, (char *)payload);
		break;
	  case OBSERVE_OK: /* server accepeted observation request */
		 printf("OBSERVE_OK: %*s\n", len, (char *)payload);
		 update_payload(&obs->addr, (char *)payload);
		break;
	  case OBSERVE_NOT_SUPPORTED:
		printf("OBSERVE_NOT_SUPPORTED: %*s\n", len, (char *)payload);
		remove_observation(addr);
		obs = NULL;
		break;
	  case ERROR_RESPONSE_CODE:
		printf("ERROR_RESPONSE_CODE: %*s\n", len, (char *)payload);
		remove_observation(addr);
		obs = NULL;
		break;
	  case NO_REPLY_FROM_SERVER:
		 printf("NO_REPLY_FROM_SERVER: "
			   "removing observe registration with token %x%x\n",
			   obs->token[0], obs->token[1]);
		remove_observation(addr);
		obs = NULL;
		break;
	}
}

static void update_payload(uip_ip6addr_t *addr, char *payload)
{
	proxying_res *s;
	struct req *r;
	
	const char *i_vol = NULL;
	const char *f_vol = NULL;

	for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
	{			
		if(uip_ipaddr_cmp(addr, &(s->obs->addr))){
			
			i_vol = strstr(payload+8, ":'");
			f_vol = strstr(i_vol+1, ".");
			
			s->volume.i_part = atoi(i_vol+2);
			s->volume.f_part = atol(f_vol+1);
			
			s->counter++;
				
			r = (struct req *)memb_alloc(&request_allocator);
			r->ID = s->ID;
			
			list_push(request_list, r);		
		
			REST.notify_subscribers(&s->res);
			return;
		}
	}
}

static void remove_observation(const char *addr)
{
	proxying_res *s;
	char temp[MAX_STRING_LEN];
	
	for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
	{			
		uip_debug_ipaddr_sprint(temp, &(s->obs->addr));
		if(strcmp(temp,addr)){
			restful_services_list = rest_get_resources();
			list_remove(restful_services_list, &s->res);
			
			list_remove(res_list, s);
				
			coap_obs_remove_observee(s->obs);
				
			memb_free(&string_allocator, (void *)s->attr);
			memb_free(&uri_allocator, (void *)s->uri);
			memb_free(&memb_res_allocator, (void *)s);
			return;
		}
	}
}
	
/* ------------------ SERVER HANDLER ------------------------------ */

static void event_handler()
{
	;
}
	
static void res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	const char *url = NULL;
	proxying_res *s;
	struct req *r;
	int length;
	
	char uri[4];
	char temp[80];
	
	int len = coap_get_header_uri_path(request, &url);
	
	if(len == 0){
		r = list_pop(request_list);	
			for(s = list_head(res_list); s!=NULL&&s->ID!=r->ID; s = list_item_next(s))
			;
			
			
			printf("OBSERVING NOTIFY: Uri: %s\n",s->uri->str);
				
			memb_free(&request_allocator, (void *)r);

	}	
	else{		
		memcpy(uri,url, 4);
		uri[3] = '\0';

		printf("STANDARD GET: Uri: %s\n", uri);
		for(s = list_head(res_list); s!=NULL; s = list_item_next(s)){
			
			if(strncmp(uri,s->uri->str,4)==0){
				break;
				//printf("%s == %s \n",uri,s->uri->str);
				//sprintf(temp, "{\"e\":[{\"v\":\"%03d\",\"u\":\"%\"}, {\"v\":\"%03d\",\"u\":\"lat\"}, {\"v\":\"%03d\",\"u\":\"lon\"}],\"bn\":\"%03d%\"}", (int)s->volume, (int)s->latitude, (int)s->longitude, s->ID);	
			}
		}	
	}
	sprintf(temp, "{'%s': {'Volume':'%d.%ld', 'Lat':'%d.%ld', 'Lon':'%d.%ld'}}", s->uri->str, s->volume.i_part, s->volume.f_part, s->latitude.i_part, s->latitude.f_part, s->longitude.i_part, s->longitude.f_part);
	
	length = strlen(temp);
			
	memcpy(buffer, temp, length);

	coap_set_header_content_format(response, APPLICATION_JSON); 
	coap_set_header_max_age(response, MAX_AGE);
	coap_set_header_etag(response, (uint8_t *) &length, 1);
	coap_set_payload(response, buffer, length);
}

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
	static uip_ipaddr_t *my_ip;
	static struct etimer timer;
	static proxying_res *s;
	static int i;
	
	PROCESS_BEGIN();
	
	my_ip = set_global_address();
	
	unsigned int num = randr(A_CODE, Z_CODE);
	
	proxy_name = (char)num;
	printf("Hi, I'm the proxy %c\n", proxy_name);
	
	rest_init_engine();
	
	printf("Activating proxy resource\n");
	rest_activate_resource(&proxy_res, "proxy_resource");
	
	servreg_hack_register(SERVICE_ID, my_ip);
	
	for(i=0;i<MAX_MOTES;i++)
		old_counters[i] = 0;
	
	etimer_set(&timer, WAIT_INTERVAL);
	while(1){
		PROCESS_WAIT_EVENT();
		
		if(etimer_expired(&timer));
		
		printf("TIMER\n");
		i = 0;
				
		for(s = list_head(res_list); s!=NULL; s = list_item_next(s)){
			if(s->counter != old_counters[i]){
				printf("%s is ALIVE\n",s->uri->str);
				old_counters[i] = s->counter;
			}
			else{
				printf("%s is DEAD\n",s->uri->str);
				//Remove resource from restful_services list
				restful_services_list = rest_get_resources();
				list_remove(restful_services_list, &s->res);
				
				//Remove proxying_resource from res_list
				list_remove(res_list, s);
				
				//Remove observing relation
				coap_obs_remove_observee(s->obs);
				
				//Free memory from memb allocators
				memb_free(&string_allocator, (void *)s->attr);
				memb_free(&uri_allocator, (void *)s->uri);
				memb_free(&memb_res_allocator, (void *)s);
			}
			i++;
		}	

		etimer_reset(&timer);
	}
	
	PROCESS_END();
}
