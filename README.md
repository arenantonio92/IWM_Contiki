# IWM_Contiki

Per il momento non ho ancora collegato il codice scritto da michele, perciò devi usare la push tramite copper per far finta che un mote si sia acceso.

QUINDI, 

1) su cooja apri già la simulazione con il file project.csc

2) su terminale vai nella cartella del progetto e digita make connect-router-cooja 

3) avvia simulazione

Al momento ci sono due proxy con tre risorse ciascuno: 

1- il proxy coap://[aaaa::c30c:0:0:6]/ ha le risorse che cominciano per lettera T

2- il proxy coap://[aaaa::c30c:0:0:c]/ ha le risorse che cominciano per lettera H

  
4) puoi aprire localhost/IWM (se hai settato le varie relazioni di observing) o tramite Copper fare observing/get sulle risorse 
Es. coap://[aaaa::c30c:0:0:6]/T01

5) se clicchi il pulsante su un mote qualunque, azzera il volume.

ps se ti esce blockOutOfScope su copper, è quelli il bug............

pps se vuoi aggiungere altri mote, ci sono già e sono quelli che hanno nome Mote (non MicheloMote).
  
