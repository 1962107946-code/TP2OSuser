# TP OS User - Partie 2

## Structure du projet

### Dossier UDP
- servudp.c : serveur UDP avec accuse de reception
- cliudp.c : client UDP qui recoit l'accuse de reception
- servbeuip.c : serveur BEUIP
- clibeuip.c : client BEUIP
- Makefile : compilation des programmes UDP et BEUIP

### Dossier Triceps
- creme.c / creme.h : librairie des fonctions reseau BEUIP
- triceps.c : interpreteur avec commandes internes
- Makefile : compilation de triceps et de libcreme.a

## Contenu realise

### Etape 1
- accuse de reception UDP entre cliudp et servudp

### Etape 2
- protocole BEUIP sur le port 9998
- identification par broadcast
- gestion d une table d utilisateurs (IP + pseudo)
- commande liste
- envoi d un message a un pseudo
- envoi d un message a tout le monde
- verification que les commandes 3, 4 et 5 viennent de 127.0.0.1
- message 0 envoye a l arret pour signaler la deconnexion

### Etape 3
- creation de la librairie creme
- commande beuip start pseudo
- commande beuip stop
- commande mess list
- commande mess to pseudo message
- commande mess all message
- compilation conditionnelle avec -DTRACE

## Compilation

### UDP
cd UDP
make

### Triceps
cd Triceps
make
