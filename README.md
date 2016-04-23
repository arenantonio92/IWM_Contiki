# IWM_Contiki

Per il momento non ho ancora collegato il codice scritto da michele, perciò devi usare la push tramite copper per far finta che un mote si sia acceso.

QUINDI, 
1) su cooja apri già la simulazione con il file project.csc
2) su terminale vai nella cartella del progetto e digita make connect-router-cooja 
3) avvia simulazione
4) tramite firefox digita coap://[aaaa::c30c:0:0:2]:5683/proxy_resource

5) In outgoing digita
  {'Ip':'[aaaa::c30c:0:0:3]', 'Lat':'123.1234567', 'Lon':'123.1234567'} e poi clicca su POST in alto (dovrebbe darti 2.05 Content), su cooja dovresti vedere OBSERVE_OK
  {'Ip':'[aaaa::c30c:0:0:4]', 'Lat':'123.1234567', 'Lon':'123.1234567'} e poi POST
  {'Ip':'[aaaa::c30c:0:0:5]', 'Lat':'123.1234567', 'Lon':'123.1234567'} e poi POST
  
6) poi puoi aprire localhost/IWM o tramite Copper fare observing/get sulle risorse /M00, /M01, /M02 

ps se ti esce blockOutOfScope su copper, è quelli il bug............
  
