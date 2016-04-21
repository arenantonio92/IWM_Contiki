#include "proxy-resource.h"

/* --------- MOTES DISCOVERING and REGISTRATION -------- */

		
static void res_post_handler(void *request, void *response, 
	uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	size_t len = 0;
	const char *ip = NULL;
	const char *lat = NULL;
	const char *lon = NULL;
	uip_ip6addr_t mote_addr;
	int success = 1;
	
	if((len = REST.get_post_variable(request,"IP",&ip))){
		printf("New mote with ip: %s\n",ip);
		if(uiplib_ip6addrconv(ip, &mote_addr)==0)
			success = 0;
	}
	else
		success = 0;
	
	/*if((len = REST.get_post_variable(request,"LAT",&lat)))
		 printf("Latitude: %s\n",lat);
	else
		success = 0;
	
	if((len = REST.get_post_variable(request,"LON",&lon)))
		 printf("Longitude: %s\n",lon);
	else 
		success = 0;
	*/
	
	if(success){
		printf("New observation relation with mote %01x\n",seq);
		proxying_res *p;
		
		if(initialized == 0){
			memb_init(&memb_res_allocator);
			memb_init(&request_allocator);
			memb_init(&string_allocator);
			memb_init(&uri_allocator);
			
			list_init(request_list);
			list_init(res_list);
			initialized = 1;
		}
		
		p = (proxying_res*)memb_alloc(&memb_res_allocator);
		if(p == NULL){
			printf("Memb error: no free objects\n");
			success = 0;
		}
		else{
			p->ID 	 = seq++;
			p->attr  = memb_alloc(&string_allocator);
			p->uri   = memb_alloc(&uri_allocator);
			
			p->latitude = 25.30;//atol(lat);
			p->longitude = 15.20;//atol(lon);
			
			register_obs(&mote_addr, p);
			init_resource(p);
			list_add(res_list, p);
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
	
	sprintf(p->attr->str, "title=\"R_M%02d\";rt=\"Text\"",p->ID);
	
	printf("Initizalizing Resource for mote %02d\n", p->ID);
	
	p->res.next 			= NULL;
	p->res.url 				= NULL;
	p->res.flags 			= IS_OBSERVABLE;
	p->res.attributes   	= p->attr->str;
	p->res.get_handler  	= res_get_handler;
	p->res.post_handler 	= NULL;
	p->res.put_handler 		= NULL;
	p->res.delete_handler 	= NULL;
	p->res.trigger 			= event_handler;


	sprintf(p->uri->str, "M%02d",p->ID);
	
	printf("Activating Resource for mote %02d\n", p->ID);
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

static void update_payload(uip_ip6addr_t *addr, char *payload)
{
	proxying_res *s;
	struct req *r;
	
	for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
	{			
		
		
		if(uip_ipaddr_cmp(addr, &(s->obs->addr))){
			
			s->volume = atol(&payload[11]);
			
			r = (struct req *)memb_alloc(&request_allocator);
			r->ID = s->ID;
			
			printf("Update payload for mote %02d\n", s->ID);
			
			list_push(request_list, r);		
			
			printf("Notify Subscribers for mote %02d\n", s->ID);

			REST.notify_subscribers(&s->res);
			return;
		}
	}
}
				
			

static void remove_object_list(const char *addr)
{
	proxying_res *s;
	char temp[MAX_STRING_LEN];
	
	for(s = list_head(res_list); s!=NULL; s = list_item_next(s))
	{			
		uip_debug_ipaddr_sprint(temp, &(s->obs->addr));
		if(strcmp(temp,addr)){
			list_remove(res_list, s);
			memb_free(&string_allocator, (void *)s->attr);
			memb_free(&uri_allocator, (void *)s->uri);
			memb_free(&memb_res_allocator, (void *)s);
			return;
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
	struct req *r;
	printf("TRIGGER\n");
	r = list_head(request_list);
	
	for(s = list_head(res_list); s!=NULL && s->ID!=r->ID; s = list_item_next(s))
		;	
	
	printf("Notify Subscribers for mote %02d\n", s->ID);
	REST.notify_subscribers(&s->res);
}
	
static void res_get_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
	const char *url;
	proxying_res *s;
	struct req *r;
	int length;
	
	
	coap_get_header_uri_path(request, &url);
	
	
	if(url == NULL){
		r = list_pop(request_list);	
		printf("I'm in the GET Handler for mote ID: %d\n",r->ID);
		for(s = list_head(res_list); s!=NULL&&s->ID!=r->ID; s = list_item_next(s))
		;
			
			printf("OBSERVING NOTIFY: Uri: %s\n",s->uri->str);
			
			char temp[80];
			
			//sprintf(temp, "{\"e\":[{\"v\":\"%03u\",\"u\":\"%\"}, {\"v\":\"%03u\",\"u\":\"lat\"}, {\"v\":\"%03u\",\"u\":\"lon\"}]}", (int)s->volume, (int)s->latitude, (int)s->longitude);
			sprintf(temp, "{'Dumpster': {'Volume':%03u, 'Lat':%03u, 'Long':%03u, 'ID':%03u}}", (int)s->volume, (int)s->latitude, (int)s->longitude, s->ID);
			length = strlen(temp);
			
			memcpy(buffer, temp, length);
				
			memb_free(&request_allocator, (void *)r);

			REST.set_header_content_type(response, REST.type.APPLICATION_JSON); 
			REST.set_header_max_age(response, MAX_AGE);
			REST.set_header_etag(response, (uint8_t *) &length, 1);
			REST.set_response_payload(response, buffer, length);
	}	
	else{
		char temp[80];
		
		strcpy(temp, url);
		temp[4] = '\0';
		
		printf("STANDARD GET: Uri: %s\n",url);
		for(s = list_head(res_list); s!=NULL; s = list_item_next(s)){
			if(strcmp(temp,s->uri->str)){
				
				//sprintf(temp, "{\"e\":[{\"v\":\"%03u\",\"u\":\"%\"}, {\"v\":\"%03u\",\"u\":\"lat\"}, {\"v\":\"%03u\",\"u\":\"lon\"}]}", (int)s->volume, (int)s->latitude, (int)s->longitude);
				sprintf(temp, "{'Dumpster': {'Volume':%03u, 'Lat':%03u, 'Long':%03u, 'ID':%03u}}", (int)s->volume, (int)s->latitude, (int)s->longitude, s->ID);
				
				length = strlen(temp);
				
				memcpy(buffer, temp, length);
				REST.set_header_content_type(response, REST.type.APPLICATION_JSON); 
				REST.set_header_max_age(response, MAX_AGE);
				REST.set_header_etag(response, (uint8_t *) &length, 1);
				REST.set_response_payload(response, buffer, length);
				continue;
			}
		}	
	}
	
}
