#include "uip-debug.h"
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
#define WAIT_INTERVAL 		30*CLOCK_SECOND

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

static struct ctimer timer;
