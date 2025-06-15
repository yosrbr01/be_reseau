#include <mictcp.h>
#include <api/mictcp_core.h>
#include <stdbool.h>

#define DEBUGING 0  //= 1 si debogage en cours

#define perte_canal 20
#define NB_TOTAL_TEST 100
bool negociation = 1;  //= 1 si phase de négociation en cours

//Tableau contenant les mic_tcp_sock référencés par leur descripteurs
#define MAX_SOCKET 100
mic_tcp_sock tab[MAX_SOCKET];
int done = 0;  //= 1 si tableau initialisé

mic_tcp_sock *get_socket(int fd){
    if (fd < 0 || fd >= MAX_SOCKET){
        return NULL;
    }
    return &tab[fd];
}

//Variables permettant le suivi des paquets
int current_seq_num = 0;
int expected_seq_num = 0;
int expected_ack_num = 0;


//Fenêtre glissante permettant de gérer le taux de perte pour une fiabilité partielle
#define window_size 10
int acceptable_loss = window_size;  //Ne pas modifier (sera modifié lors de la négociation)
int window[window_size] = {0};

void update_window(int ack_num_received, bool received){
    int index = ack_num_received % window_size;  //Evite un segmentation fault
    window[index] = received;
    if (received == 1 && index == expected_ack_num){  //Actuel pdu reçu
        if (DEBUGING) printf("Actuel pdu reçu\nack_num_received = expected_ack_num = %d\n", index);
    } else if (received == 0 && index == expected_ack_num) {  //On a eu un timeout
        if (DEBUGING) printf("On a eu un timeout\nexpected_ack_num = %d\n", index);
    } else {  //Précédent pdu reçu
        if (DEBUGING) printf("Ancien pdu reçu\nack_num_received = %d\nexpected_ack_num = %d\n", index, expected_ack_num % window_size);
    }
}

// Retourne 0 si la perte est acceptable et 1 sinon
bool evaluate_loss_rate(){
        int total_received = 0;
        for (int i = 0; i < window_size; i++){
        total_received += window[i];
        }
    if ((window_size - total_received) <= acceptable_loss){
        if (DEBUGING) printf("Perte acceptable. Taux de perte actuel: %d / %d ; Taux acceptable: %d / %d\n", window_size - total_received, window_size, acceptable_loss, window_size);
        return 0;
    }else{
        if (DEBUGING) printf("Perte non acceptable. Taux de perte actuel: %d / %d ; Taux acceptable: %d / %d\n", window_size - total_received, window_size, acceptable_loss, window_size);
        return 1;
    }
}



/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
    //Initialiser le tableaux des sockets une seule fois
    if (!done){
        for (int i = 0; i < MAX_SOCKET; i++){
            int fd = initialize_components(sm);
            tab[i].fd = fd;
            tab[i].state = CLOSED;
        }
    } done = 1;

   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
   set_loss_rate(perte_canal);
   if(result<0){
        return -1;
   }
   int fd_place = -1;
    for (int i = 0; i < MAX_SOCKET; i++) {
        if (tab[i].state == CLOSED) { 
            fd_place = i;
            break;
        }
    } 
    if (fd_place == -1){
        return -1;
    }
    tab[fd_place].fd = result;
    tab[fd_place].state = IDLE;
    return tab[fd_place].fd;
}


/* 
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    //Récupérer le socket grâce au descripteur
    mic_tcp_sock *s = get_socket(socket);
    if (s == NULL){
        return -1;
    }
    //Associer une addresse au socket
    s->local_addr=addr;
    return 0;
}


/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
/*int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    return 0;
}*/
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    //Récupérer le socket grâce au descripteur
    mic_tcp_sock *s = get_socket(socket);
    if (s == NULL) return -1;
    s->state = IDLE;
    return 0;
}


/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    //Récupérer le socket grâce au descripteur
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    mic_tcp_sock *s = get_socket(socket);
    if (s == NULL){
        return -1;
    }
    s->remote_addr = addr;
    
    //Construire et envoyer PDU SYN
    mic_tcp_pdu syn_pdu;
    syn_pdu.header.syn = 1;
    syn_pdu.header.ack = 0;
    syn_pdu.header.source_port = s->local_addr.port;
    syn_pdu.header.dest_port = addr.port;
    syn_pdu.payload.size=0;
    s->state = SYN_SENT;
    IP_send(syn_pdu,addr.ip_addr);

    //Attendre la réponse SYN-ACK
    mic_tcp_pdu syn_ack_pdu;
    mic_tcp_sock_addr server_addr;
    server_addr.ip_addr.addr_size = 16; 
    server_addr.ip_addr.addr = (char*) malloc(server_addr.ip_addr.addr_size);
    if (server_addr.ip_addr.addr == NULL) {
        perror("malloc échoué pour server_addr.ip_addr.addr");
        return -1;
    }
    syn_ack_pdu.payload.size = 0;
    //server_addr.ip_addr.addr_size = 0;
    unsigned long timeout = 1000;
    IP_recv(&syn_ack_pdu, &s->local_addr.ip_addr,&server_addr.ip_addr,timeout);

    //Envoyer PDU ACK final
    mic_tcp_pdu ack_pdu;
    ack_pdu.header.syn = 0;
    ack_pdu.header.ack = 1;
    ack_pdu.payload.size = 0;
    ack_pdu.header.source_port =s->local_addr.port;
    ack_pdu.header.dest_port = addr.port;
    IP_send(ack_pdu,addr.ip_addr);

    s->state = CONNECTED;
    free(server_addr.ip_addr.addr);

    //Négociation du taux de perte acceptable
    while(negociation){
        //Demander l'avis du client
        int loss_suggestion = -1;
        printf("Entrez le pourcentage de perte toléré (entier entre 0 et 100): \n");
        while (!(loss_suggestion >= 0)){
            scanf("%d", &loss_suggestion);
        }

        //Tester empiriquement si la proposition est plausible
        int loss_counter = 0;
        int res;
        char test_mesg = 'a';
        for (int i = 0; i < NB_TOTAL_TEST; i++){
            if ((res = mic_tcp_send(socket, &test_mesg, 1)) == -1){
                loss_counter ++;
                if (DEBUGING) printf("Perte. loss_counter = %d\n", loss_counter);
            }
        }
        printf("Résultat du test canal: %d pertes sur %d\n", loss_counter, NB_TOTAL_TEST);

        if (loss_suggestion >= 0.95*loss_counter){
            acceptable_loss = (loss_suggestion*window_size)/100;
            printf("Taux de perte de %d/100 accepté.\n", loss_suggestion);
            if (DEBUGING) printf("acceptable_loss set to %d\n", acceptable_loss);
            test_mesg = 'b';
            do{
                res = mic_tcp_send(socket, &test_mesg, 1);
            } while (res == -1);
            negociation = 0;
            current_seq_num = 0;
            expected_ack_num = 0;
        } else {
            printf("Taux de perte choisi trop faible.\nVeuillez en proposer un nouveau plus grand que %d/100\n", loss_suggestion);
        }
    }

    return 0;
}


/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur return -1;
 */
int mic_tcp_send (int socket, char* mesg, int mesg_size)
{
    //Récupérer le socket grâce au descripteur
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);
    mic_tcp_sock *s = get_socket(socket);
    if (s == NULL) return -1;

    //Construction du PDU a envoyer
    mic_tcp_pdu pdu;
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;
    pdu.header.source_port = s->local_addr.port;
    pdu.header.dest_port = s->remote_addr.port;
    pdu.header.seq_num = current_seq_num;
    pdu.header.syn = 0;
    pdu.header.ack = 0;

    //Construction de l'ACK à recevoir
    mic_tcp_pdu ack_pdu;
    unsigned long timeout = 1000;
    int result = -1;

    if (negociation){
        pdu.header.ack = 1;
    }

    //Envoi du PDU et attente du ACK
    while (1) {
        IP_send(pdu, s->remote_addr.ip_addr);
        if (DEBUGING) printf("Envoi pdu avec seq_num = %d\n", pdu.header.seq_num);

        result = IP_recv(&ack_pdu, &s->local_addr.ip_addr, &s->remote_addr.ip_addr, timeout);
        if (DEBUGING) printf("Resultat du IP_recv = %d\n", result);
        if (result >= 0 && ack_pdu.header.ack == 1) {  //ACK reçu
            if (DEBUGING) printf("ACK recu\n");
            if (ack_pdu.header.ack_num == current_seq_num) {  //Actuel pdu reçu
                update_window(current_seq_num, 1);
                break;
            } else {
                update_window(ack_pdu.header.ack_num, 1);  //Ancien pdu reçu
            }
        } else {  //Timeout
            if (DEBUGING) printf("Timeout\n");
            update_window(current_seq_num, 0);
            if (evaluate_loss_rate() == 0) {  //Perte acceptée
                break;
            }
        }
    }
    current_seq_num = (current_seq_num + 1);
    expected_ack_num = (expected_ack_num + 1);
    return result;
}


/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_payload payload;
    payload.data = mesg;
    payload.size = max_mesg_size;
    int effectivement_lu_du_buffer = app_buffer_get(payload);
    return effectivement_lu_du_buffer;
}


/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    mic_tcp_sock *s = get_socket(socket);
    if(s == NULL){
        return -1;
    }
    s->state = CLOSED;
    return 0;
}


/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_ip_addr local_addr, mic_tcp_ip_addr remote_addr)
{
    printf("[MIC-TCP] Appel de la fonction: %s\n", __FUNCTION__);

    ////Récupérer le socket grâce à l'addresse donnée
    int socket_fd = -1;
    for (int i = 0; i < MAX_SOCKET; i++) {
        if (tab[i].state != CLOSED && tab[i].local_addr.port == pdu.header.dest_port) {
            socket_fd = i;
            break;
        }
    }
    if (socket_fd == -1) {
        printf("Socket non trouvé pour le port %d\n", pdu.header.dest_port);
        return;
    }
    mic_tcp_sock *s = &tab[socket_fd];

    //Récéption d'un SYN
    if (DEBUGING) printf("Récéption d'un SYN\n");
    if (s->state == IDLE && pdu.header.syn == 1) {
        s->remote_addr.ip_addr.addr = remote_addr.addr;
        s->remote_addr.ip_addr.addr_size = remote_addr.addr_size;
        s->remote_addr.port = pdu.header.source_port;
        s->state = SYN_RECEIVED;
        
        //Construction et envoi d'un SYN-ACK
        mic_tcp_pdu syn_ack;
        syn_ack.header.syn = 1;
        syn_ack.header.ack = 1;
        syn_ack.header.seq_num = 0;
        syn_ack.header.ack_num = 0;
        syn_ack.header.source_port = pdu.header.dest_port;
        syn_ack.header.dest_port = pdu.header.source_port;
        syn_ack.payload.size = 0;
        IP_send(syn_ack, remote_addr);
        return;

    //Récéption d'un ACK (pour la 1ere fois)
    } else if (s->state == SYN_RECEIVED && pdu.header.ack == 1) {
        if (DEBUGING && !negociation) printf("Récéption d'un ACK (pour la 1ere fois)\n");
        //Début des négociations
        if(negociation){
            if (DEBUGING) printf("Début des négociations\n");
            if (DEBUGING) printf("test_msg = %s\ntest_size = %d\n", pdu.payload.data ,pdu.payload.size);
            mic_tcp_pdu ack_test;
            ack_test.header.syn = 0;
            ack_test.header.ack = 1;
            ack_test.header.ack_num = pdu.header.seq_num;
            ack_test.header.source_port = pdu.header.dest_port;
            ack_test.header.dest_port = pdu.header.source_port;
            ack_test.payload.size = 0;
    
            IP_send(ack_test, remote_addr);

            if (*pdu.payload.data == 'b'){  //Arrêt de la négociation lorsqu'on reçoit un 'b'
                if (DEBUGING) printf("Fin de la négociation\n");
                negociation = 0;
                s->state = CONNECTED;
                expected_seq_num = 0;
                return;
            }
            //Fin des négociations + Mise en état 'connecté'
        }
        return;

    //Récéption d'un ACK (pas pour la 1ere fois)
    } else if (s->state == CONNECTED && pdu.header.ack == 1) {
        if (!negociation) {
            if (DEBUGING) printf("Récéption d'un ACK (pas pour la 1ere fois)\n");
            //Renvoyer PDU ACK
            mic_tcp_pdu ack_pdu;
            ack_pdu.header.syn = 0;
            ack_pdu.header.ack = 1;
            ack_pdu.payload.size = 0;
            ack_pdu.header.source_port = pdu.header.dest_port;
            ack_pdu.header.dest_port = pdu.header.source_port;
            IP_send(ack_pdu,remote_addr);
        }

    //Récéption d'un PDU DATA
    } else if (s->state == CONNECTED && pdu.header.ack == 0) {
        if (DEBUGING) printf("Récéption d'un PDU DATA\nEnvoi de l'ACK\n");
        mic_tcp_pdu ack;
        ack.header.syn = 0;
        ack.header.ack = 1;
        ack.header.source_port = pdu.header.dest_port;
        ack.header.dest_port = pdu.header.source_port;
        ack.payload.size = 0;

        if (pdu.header.seq_num == expected_seq_num) {
            if (DEBUGING) printf("Bon PDU reçu. seq_num = %d\nEnvoi d'un ACK\n", expected_seq_num);
            app_buffer_put(pdu.payload);
            ack.header.ack_num = pdu.header.seq_num;
            expected_seq_num = (pdu.header.seq_num + 1);
        } else if (pdu.header.seq_num < expected_seq_num) {
            if (DEBUGING) printf("Ancien PDU reçu. seq_num = %d ; expected_seq_num = %d\nOn renvoit un ack mais pas de buffer_put\n", pdu.header.seq_num, expected_seq_num);
            ack.header.ack_num = pdu.header.seq_num; // duplicate ack
        } else {
            if (DEBUGING) printf("Une perte a été acceptée. seq_num = %d\nEnvoi d'un ACK\n", expected_seq_num);
            app_buffer_put(pdu.payload);
            ack.header.ack_num = pdu.header.seq_num;
            expected_seq_num = (pdu.header.seq_num + 1);
        }

        if (DEBUGING) printf("ACK envoyé avec ack_num = %d\n", ack.header.ack_num);
        IP_send(ack, remote_addr);
    }
}
