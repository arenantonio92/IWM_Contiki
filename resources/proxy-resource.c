#include "proxy-resource.h"


/* --------- MOTES DISCOVERING and REGISTRATION -------- */

	
RESOURCE(proxy_resource,
		"title=\"Fake resource, to warn proxy of new motes\";rt=\"ProxyResource\"",
		NULL,
		res_post_handler,
		res_post_handler,
		NULL);
		
static void res_post_handler(void *request, void *response, 
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
		proxying_res *p;
		
		p = (proxying_res*)malloc(sizeof(proxying_res));
		if(initialized == 0){
			list_init(res_list);
			initialized = 1;
		}
		p->ID = seq++;
		register_obs(&mote_addr, p);
		p->is_fresh = 0;
		init_resource(p);
		list_add(res_list, p);
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
	
	char title[40];
	sprintf(title, "title=\"Resource M%02d\";rt=\"Text\"",p->ID);
	
	 printf("Initizalizing Resource for mote %02d\n", p->ID);
	
	p->res.next 		= NULL;
	p->res.url 			= NULL;
	p->res.flags 		= IS_OBSERVABLE;
	MYSTRCPY(p->res.attributes, title);
	p->res.get_handler  = res_get_handler;
	p->res.post_handler = NULL;
	p->res.put_handler 	= NULL;
	p->res.delete_handler = NULL;
	p->res.trigger 		= event_handler;

	sprintf(title, "Res_M%02d",p->ID);
	MYSTRCPY(p->res.url, title);
	
	printf("Activating Resource for mote %02d\n", p->ID);
	rest_activate_resource(&p->res, p->res.url);
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
		 update_payload(addr, (char *)payload);
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
	proxying_res *s;
	char temp[30];
	
	for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
	{			
		uip_debug_ipaddr_sprint(temp, &(s->obs->addr));
		if(strcmp(temp,addr)){
			strcpy(s->payload, (char*)payload);
			s->is_fresh = 1;
			 printf("TRIGGER\n");
			s->res.trigger();
		}
	}
}
				
			

static void remove_object_list(const char *addr)
{
	proxying_res *s;
	char temp[50];
	for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
	{			
		uip_debug_ipaddr_sprint(temp, &(s->obs->addr));
		if(strcmp(temp,addr)){
			list_remove(res_list, s);
			free(s);
		}
	}
}

static void remove_observation(coap_observee_t *obs, const char *addr)
{
	coap_obs_remove_observee(obs);
	remove_object_list(addr);
}
	
/* ------------------ SERVER HANDLER ------------------------------ */

static void event_handler()
{
	proxying_res *s;
	for(s = list_head(res_list); s!=NULL && s->is_fresh==0; s = list_item_next(s))
		;	

	REST.notify_subscribers(&s->res);
}
	
static void res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	proxying_res *s;
	int length;
	
	for(s = list_head(res_list); s!=NULL && s->is_fresh==0; s = list_item_next(s))
		;
	
	 printf("I'm in the GET Handler\n");
	
	if(s!=NULL){
		 printf("Observe notify\n");
		s->is_fresh = 0;
		length = strlen(s->payload);
		memcpy(buffer, s->payload, length);

		REST.set_header_content_type(response, REST.type.APPLICATION_JSON); 
		REST.set_header_max_age(response, MAX_AGE);
		REST.set_header_etag(response, (uint8_t *) &length, 1);
		REST.set_response_payload(response, buffer, length);	
	}
	
	else{
		const char *url;
		REST.get_url(request, &url);
		 printf("Standard GET:%s\n",url);
		for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
		{
			if(strcmp(s->res.url, url)){
				length = strlen(s->payload);
				memcpy(buffer, s->payload, length);

				REST.set_header_content_type(response, REST.type.APPLICATION_JSON); 
					
				REST.set_header_etag(response, (uint8_t *) &length, 1);
				REST.set_response_payload(response, buffer, length);	
				continue;
			}
		}
	}
}
	
