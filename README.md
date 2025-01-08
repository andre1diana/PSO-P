# PROCESSING SYSTEM
  Andrei Diana, Leu Cătălin<br />
  C113B


## 1. Processing System:
1. Server - centralizat care primeste requesturi cu sarcini de executie (prin socket)
2. Agent - instalat pe o statie, se înregistrează în server, primește sarcini (prin sockets).

## 2. Functionalitati:
  
SERVER:
- Primește sarcini de la client (task-uri sub forma unor comenzi urmate de argumente); ex : bash ./resources/script.sh
- La pronirea serverului, acest asteapta automat conexiuni prin socket; agenții se conecteaza automat la server la pornirea lor
- Distribuie taskuri catre primul agent liber(queue de agenti)
- Primeste rezultatul de la agent cand acesta termina
- Serverul mentine in memorie o coada de agenti, una de clienti si o coada de taskuri ce trebuie executate
- Trimitem rezultatul la client
    - Return-ul rezultatului se efectueaza la primirea mesajului TASK_STATUS

AGENT:
- Primeste sarcini de la server
- Ruleaza executabil (rezolva sarcina)
- Returneaza rezultat la server

CLIENT:
- Poate incarca un executabil 
- Completeaza argumentele
- Trimite catre server informatiile
- Metoda de conectare la server

AUTENTIFICARE:
- Agentii se vor conecta automat la server pe baza unui ID.

## 4. Gestiunea task-urilor
- Structuri de date stocate in server care vor contine informatii despre Clienti, Agenti si Task-uri.
- In structura de date a Agentilor se vor afla flag-uri despre capabilitatile lor si disponibilitate.
- In structura de date a Task-urilor se vor afla flag-uri despre cerintele lor.
- Cu ajutorul acestor structuri, Server-ul va trimite Task-uri agentilor disponibili si capabili sa execute Task-ul respectiv.

## 5. Instalare si rulare
### Descărcare proiect
```bash
git clone https://github.com/username/PSO-P.git
cd PSO-P

Compilare
Proiectul folosește un Makefile pentru compilare. Pentru a compila toate componentele:

 > make all

Rulare
Pornește serverul:
 > ./server
Pornește unul sau mai mulți agenți în terminale separate:
 > ./agent <agent_conf_file> 
Pornește unul sau mai mulți clienți în terminale separate:
 > ./client <client_id>
