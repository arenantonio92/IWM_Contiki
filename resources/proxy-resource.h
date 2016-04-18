#include "contiki.h"
#include "contiki-net.h"
#include "uip-debug.h"
#include "string.h"
#include "rest-engine.h"
#include "er-coap-engine.h"
#include "er-coap.h"
#include "lib/list.h"
#include "stdlib.h"

#define MAX_MOTES 128
#define MAX_PAYLOAD 50
#define REMOTE_PORT     UIP_HTONS(COAP_DEFAULT_PORT)
#define OBS_RESOURCE_URI "dumpster" 

/* --------- PROXYING RESOURCES ------------------------ */

static uint16_t seq = 0;
static uint8_t initialized = 0;

extern resource_t proxy_resource;

struct proxying_res{
	struct proxying_res *next;
	uint16_t ID;
	char payload[MAX_PAYLOAD];
	coap_observee_t *obs;	
};

LIST(res_list);

/* FUNCTION DECLARATION */

//Register a new observation
static void 
register_obs(uip_ip6addr_t *addr, struct proxying_res *p);

//Remove a specific observation
static void remove_observation(coap_observee_t *obs, const char *addr);

//Handle the response to the observe request
static void
notification_callback(coap_observee_t *obs, void *notification,
                      coap_notification_flag_t flag);
                      
//Copy the new payload in the correct element list
static void update_payload(const char *addr, char *payload);

//Remove the mote from observing mote list, specified by the IPaddress
static void remove_object_list(const char *addr);
	



