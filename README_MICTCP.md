README — MICTCP

Étudiantes:
Iness NOUIGA
Yosr BEN ROMDHANE

Dépôt Github:
https://github.com/yosrbr01/be_reseau.git

Compilation:
make

Usage:
./tsock_texte [-p|-s destination] port
./tsock_video [-t (tcp|mictcp)][-p|-s]

---------------------------------------------------------------------------

Notes d'utilisation:

- Ce qui peut être modifier:
        - perte_canal (émulation du taux de perte du canal)
        - NB_TOTAL_TEST (nombre de message envoyé lors du test de la fiabilité du canal)
        - MAX_SOCKET (nombre maximum de socket stockés)
        - window_size (taille de la fenêtre glissante)
        - DEBUGING (=1 si on veut voir les messages qui aident au debogage)

- Ce qui ne devrait pas être modifier:
        - acceptable_loss = window_size; (sera modifier automatiquement lors de la negociation du taux de fiabilité)
        - done = 0 (permet d'initialiser le tableau des socket une seule fois)
        - negociation = 1 (permet de savoir lorsqu'on a fini la négociation du taux de perte accepté)
        - current_seq_num = 0 ; expected_seq_num = 0 ; expected_ack_num = 0 (initialisations des numeros de sequences et d'acquitement)

---------------------------------------------------------------------------

Commentaires sur certaines versions de MICTCP:


Fiabilité partielle (MICTCP v3):

Nous utilisons une fenêtre glissante de taille fixe pour tolérer un certain pourcentage de pertes.
La transmission continue tant que le taux de perte mesuré dans cette fenêtre est inférieur au seuil acceptable.


Négociation du taux de perte (MICTCP v4.1):

Une négociation est effectuée lors de l'établissement de la connexion:
- Le client propose un taux de perte toléré (en pourcentage)
- Un test est effectué avec l'envoi de plusieurs messages pour mesurer les pertes réelles
- Si la perte mesurée est compatible avec le seuil demandé, la communication démarre, sinon, on redemande un taux plus élevé


Asynchronisme application/protocole (MICTCP v4.2): 

Dans cette version, la réception des PDU est entièrement gérée de manière asynchrone dans la fonction process_received_PDU(), sans appel direct à IP_recv() dans mic_tcp_accept(). Cela permet d’accepter les connexions entrantes dès qu’un PDU SYN est reçu, même si l’application n’a pas encore appelé accept(). 


Bonus:

Contrairement à MICTCP v2, qui ne gère ni les pertes ni l’asynchronisme, et à TCP classique, qui impose un enchaînement strict des appels applicatifs (notamment un accept bloquant côté serveur), MICTCP v4.2 traite les PDU entrants dès leur arrivée via process_received_PDU, sans attendre l’appel à accept(). Il intègre aussi une négociation dynamique du taux de perte, absente des versions précédentes. Ces deux améliorations permettent à MICTCP v4.2 d’être plus réactif et mieux adapté aux réseaux instables ou aux applications temps réel, où tolérer quelques pertes vaut mieux que subir des délais excessifs.

---------------------------------------------------------------------------

Remarque:

- La phase de test peut prendre du temps si le NB_TOTAL_TEST est élevé.

- tsock_video fonctionnait correctement avec mictcp v3.
À partir de la version 4, il plante systématiquement dès que le serveur entre le taux de perte toléré. Plus précisément, le blocage se produit au moment de la lecture de cette valeur via un scanf dans la fonction mic_tcp_connect.
Des tests avec des printf ont montré que l’exécution du programme s’interrompt exactement à cette ligne, sans passer à la suite du code (l’envoi des messages de test pour la négociation).

---------------------------------------------------------------------------
