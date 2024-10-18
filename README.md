# PROCESSING SYSTEM
  Andrei Diana, Leu Cătălin<br />
  C113B


## 1. Processing System:
1. Server centralizat care primeste requesturi cu sarcini de executie (prin socket)
2. Agent - se instaleaza pe o statie, se înregistrează în server, primește sarcini (prin socketsi).

## 2. Functionalitati:
  
SERVER:
- Primește sarcini de la client
- La pronirea serverului asteapta automat conexiuni prin socket; agenții se conecteaza automat la server 
- Distribuie catre primul agent liber(queue de agenti)
- Primeste rezultatul de la agent cand acesta termina
- Agentul a terminat - il adaugam in coada
- Trimitem rezultatul la client

AGENT:
- Primeste sarcini de la server
- Ruleaza executabil (rezolva sarcina)
- Returneaza rezultat la server

CLIENT:
- Interfata grafica
- Poate incarca un executabil 
- Completeaza argumentele
- Trimite catre server informatiile
- Metoda de conectare la server

AUTENTIFICARE:
- Clientii se vor conecta si autentifica pe baza user name-ului si parolei. Clientii vor dispune de facilitati de a isi crea un cont.
- Agentii se vor conecta automat la server pe baza unui ID. Un administrator se va ocupa de adaugarea agentilor si gestionarea ID-urilor.

## 3. Agent placa ESP32
