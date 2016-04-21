#include "contiki.h"
#include "contiki-net.h"
#include "uip-debug.h"
#include "string.h"
#include "rest-engine.h"
#include "er-coap-engine.h"
#include "er-coap.h"
#include "lib/list.h"
#include "lib/memb.h"

#define MAX_MOTES 			3
#define MAX_PAYLOAD 		30
#define MAX_STRING_LEN		35
#define MAX_AGE 			600
#define REMOTE_PORT     	UIP_HTONS(COAP_DEFAULT_PORT)
#define OBS_RESOURCE_URI 	"dumpster" 


/* --------- PROXYING RESOURCES ------------------------ */

static uint16_t seq = 0;
static uint8_t initialized = 0;


/* List of event observable resources */
LIST(res_list);
LIST(request_list);

struct string{
	char str[MAX_STRING_LEN];
};

struct req{
	struct req *next;
	uint16_t ID;
};

struct proxying_res{
	struct proxying_res *next;
	uint16_t ID;
	char payload[MAX_PAYLOAD];
	resource_t res;
	coap_observee_t *obs;	
	struct string *attr;
	struct string *uri;
};

typedef struct proxying_res proxying_res;



//Memory management allocator definition for proxy resources

MEMB(memb_res_allocator, proxying_res, MAX_MOTES);
MEMB(request_allocator, struct req, MAX_MOTES);
MEMB(string_allocator, struct string, MAX_MOTES);
MEMB(uri_allocator, struct string, MAX_MOTES);



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

//Remove a specific observation
static void remove_observation(coap_observee_t *obs, const char *addr);

//Handle the response to the observe request
static void
notification_callback(coap_observee_t *obs, void *notification,
                      coap_notification_flag_t flag);
                      
//Copy the new payload in the correct element list
static void update_payload(uip_ip6addr_t *addr, char *payload);

//Remove the mote from observing mote list, specified by the IPaddress
static void remove_object_list(const char *addr);

//Initialize a new resource for Java Cliente
static void init_resource(proxying_res *p);

//Event resource handler
static void event_handler();

//Get Handler
static void res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
	



