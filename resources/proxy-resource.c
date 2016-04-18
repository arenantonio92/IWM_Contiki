#include "proxy-resource.h"


/* --------- MOTES DISCOVERING and REGISTRATION -------- */
static void res_put_handler(void *request, void *response, 
	uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
	
RESOURCE(proxy_resource,
		"title=\"Fake resource, to warn proxy of new motes\";rt=\"ProxyResource\"",
		NULL,
		res_put_handler,
		res_put_handler,
		NULL);
		
static void res_put_handler(void *request, void *response, 
	uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	size_t len = 0;
	const char *ip = NULL;
	uip_ip6addr_t mote_addr;
	int success = 1;
	
	if((len = REST.get_post_variable(request,"IP",&ip))){
		printf("New mote with ip: %s\n",ip);
		if(uiplib_ip6addrconv(ip, &mote_addr)==0)
			success = 0;
	}
	
	if(success){
		printf("New observation relation with mote %01x\n",seq);
		static struct proxying_res p;
		if(initialized == 0){
			list_init(res_list);
			initialized = 1;
		}
		p.ID = seq++;
		register_obs(&mote_addr, &p);
		list_add(res_list, &p);
	}
	
	if(!success){
		REST.set_response_status(response, REST.status.BAD_REQUEST);
	}
}

/* ------------ OBSERVING FUNCTION -------------------- */
static void 
register_obs(uip_ip6addr_t *addr, struct proxying_res *p)
{
	p->obs = coap_obs_request_registration(addr, REMOTE_PORT,
                        OBS_RESOURCE_URI, notification_callback, NULL);
}

static void
notification_callback(coap_observee_t *obs, void *notification,
                      coap_notification_flag_t flag)
{
	int len = 0;
	const uint8_t *payload = NULL;
	printf("Notification handler\n");
	char addr[30];
	uip_debug_ipaddr_sprint(addr, &(obs->addr));
	printf("Observee addr: %s\n", addr);
	printf("Observee URI: %s\n", obs->url);
	
		
	if(notification){
		len = coap_get_payload(notification, &payload);
	}
	
	switch(flag) {
	  case NOTIFICATION_OK:
		printf("NOTIFICATION OK: %*s\n", len, (char *)payload);
		update_payload(addr, (char *)payload);
		break;
	  case OBSERVE_OK: /* server accepeted observation request */
		printf("OBSERVE_OK: %*s\n", len, (char *)payload);
		break;
	  case OBSERVE_NOT_SUPPORTED:
		printf("OBSERVE_NOT_SUPPORTED: %*s\n", len, (char *)payload);
		remove_object_list(addr);
		obs = NULL;
		break;
	  case ERROR_RESPONSE_CODE:
		printf("ERROR_RESPONSE_CODE: %*s\n", len, (char *)payload);
		remove_object_list(addr);
		obs = NULL;
		break;
	  case NO_REPLY_FROM_SERVER:
		printf("NO_REPLY_FROM_SERVER: "
			   "removing observe registration with token %x%x\n",
			   obs->token[0], obs->token[1]);
		remove_object_list(addr);
		obs = NULL;
		break;
  }
}

static void update_payload(const char *addr, char *payload)
{
	struct proxying_res *s;
	char temp[50];
	
	for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
	{			
		uip_debug_ipaddr_sprint(temp, &(s->obs->addr));
		if(strcmp(temp,addr)){
			strcpy(s->payload, (char*)payload);
			return;
		}
	}
}
				
			

static void remove_object_list(const char *addr){
	struct proxying_res *s;
	char temp[50];
	for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
	{			
		uip_debug_ipaddr_sprint(temp, &(s->obs->addr));
		if(strcmp(temp,addr)){
			list_remove(res_list, s);
			free(s);
			return;
		}
	}
}

static void remove_observation(coap_observee_t *obs, const char *addr){
	coap_obs_remove_observee(obs);
	remove_object_list(addr);
}
	
/* ------------------ SERVER HANDLER ------------------------------ */





