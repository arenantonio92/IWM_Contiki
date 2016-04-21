CONTIKI_PROJECT = proxy

all: $(CONTIKI_PROJECT)

PROJECT_SOURCEFILES += proxy-resource.c

SMALL=1
UIP_CONF_IPV6=1

CONTIKI=/home/user/contiki

APPS += er-coap
APPS += rest-engine
APPS += servreg-hack


CFLAGS += -DUIP_CONF_IPV6_RPL
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"


CFLAGS+= -DRPL_CONF_DIO_REDUNDANCY=1
CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include


$(CONTIKI)/tools/tunslip6:	$(CONTIKI)/tools/tunslip6.c
	(cd $(CONTIKI)/tools && $(MAKE) tunslip6)

connect-router:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 aaaa::1/64

connect-router-cooja:	$(CONTIKI)/tools/tunslip6
	sudo $(CONTIKI)/tools/tunslip6 -a 127.0.0.1 -p 60001 aaaa::1/64

connect-router-native:	$(CONTIKI)/examples/ipv6/native-border-router/border-router.native
	sudo $(CONTIKI)/exmples/ipv6/native-border-router/border-router.native -a 127.0.0.1 -p 60001 aaaa::1/64

connect-minimal:
	sudo ip address add fdfd::1/64 dev tap0 
