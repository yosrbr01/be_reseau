#include <mictcp.h>
#include <api/mictcp_core.h>


//Tableau contenant les mic_tcp_sock référencés par leur fd
#define MAX_SOCKET 100
mic_tcp_sock tab[MAX_SOCKET];
int done = 0;  // Tableau initialisé si 1

mic_tcp_sock *get_socket(int fd){
    if (fd < 0 || fd >= MAX_SOCKET){
        return NULL;
    }
    return &tab[fd];
}

int current_seq_num = 0;
int expected_seq_num = 0;

int taux_acceptable = 3;


/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
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
   set_loss_rate(20);
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
   mic_tcp_sock *s = get_socket(socket);
   if (s == NULL){
        return -1;
    }
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
    mic_tcp_sock *s = get_socket(socket);
    if (s == NULL) return -1;
    //Attendre pdu syn client
    mic_tcp_pdu syn_pdu;
    mic_tcp_sock_addr client_addr;
    unsigned long timeout = 1000;
    int taille_recu = IP_recv(&syn_pdu,  &s->local_addr, &client_addr, timeout);

    //Envouer PDU SYN ACK
    mic_tcp_pdu syn_ack;
    syn_ack.header.syn = 1;
    syn_ack.header.ack = 1;
    syn_ack.payload.size = 0;
    syn_ack.header.source_port = s->local_addr.port;
    syn_ack.header.dest_port = syn_pdu.header.source_port;

    // Attendre PDU ACK final du client
    mic_tcp_pdu ack_pdu;
    mic_tcp_sock_addr addr_client;
    int recu = IP_recv(&ack_pdu, &s->local_addr, &addr_client, timeout);

    s->state = CONNECTED;
    return 0;
}



/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    mic_tcp_sock *s = get_socket(socket);
    if (s == NULL){
        return -1;
    }
    s->remote_addr = addr;
    
     //Construire et envoyer PDU SYN
    printf("etape1\n");
    mic_tcp_pdu syn_pdu;
    syn_pdu.header.syn = 1;
    syn_pdu.header.ack = 0;
    syn_pdu.header.source_port = s->local_addr.port;
    syn_pdu.header.dest_port = addr.port;
    syn_pdu.payload.size=0;
    printf("etape2\n");
    int syn_sent = IP_send(syn_pdu,addr.ip_addr);

    //attendre la réponse SYN ACK
    printf("etape3\n");
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
    printf("etape4\n");
    int taille = IP_recv(&syn_ack_pdu, &s->local_addr,&server_addr,timeout);

    //envoyer PDU ACK final
    printf("etape5\n");
    mic_tcp_pdu ack_pdu;
    ack_pdu.header.syn = 0;
    ack_pdu.header.ack = 1;
    ack_pdu.payload.size = 0;
    ack_pdu.header.source_port =s->local_addr.port;
    ack_pdu.header.dest_port = addr.port;
    printf("etape6\n");
    int ack_sent = IP_send(ack_pdu,addr.ip_addr);

    s->state = CONNECTED;
    free(server_addr.ip_addr.addr);

    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur return -1;
 */
int mic_tcp_send (int socket, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_sock *s = get_socket(socket);
   if (s == NULL){
        return -1;
    }

    // Construire PDU
    printf("on construit le pdu");
    mic_tcp_pdu pdu;

    pdu.header.source_port = s->local_addr.port;
    pdu.header.dest_port = s->remote_addr.port;
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;
    pdu.header.ack = 0;
    pdu.header.seq_num = current_seq_num;

     unsigned long timeout = 1000;
     mic_tcp_pdu pdu_ack;
     int result;
    // Envoyer pdu et attendre ACK (Stop & Wait)
    do{
        IP_send(pdu, s->remote_addr.ip_addr);
        int result = IP_recv(&pdu_ack, &s->local_addr, &s->remote_addr, timeout);
        printf("Résultat de IP_recv : %d\n", result);
        if(result >= 0) {
            printf("On a reçu un pdu_ack\n");
        } else {
            printf("Un timeout s'est produit\n");

            // update_window(current_pointer);
            // if (int acceptable = taux_perte() > taux_acceptable){
            //     break;
            // }

        }
        printf("On a envoyé pdu et on att le ack\n");
        printf("On a reçu un pdu_ack avec: header.ack = %d et ack_num = %d; on avait current_seq_num = %d\n", pdu_ack.header.ack, pdu_ack.header.ack_num, current_seq_num);

        // si on recoit un ack
        //     si bon num_ack
        //         update_window(num_ack); // ca sera le bon ack
        //         break;
        //     sinon
        //         update_window(num_ack); // ca sera un precedent ack

    } while (!(pdu_ack.header.ack == 1 && pdu_ack.header.ack_num == current_seq_num));

    // Alterner le num de sequence
    current_seq_num = (current_seq_num + 1) %2; // il va falloir mettre %window_size
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
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
     // Trouver le socket correspondant
    int socket_fd = -1;
    for (int i = 0; i < MAX_SOCKET; i++) {
        if (tab[i].state != CLOSED && 
            tab[i].local_addr.port == pdu.header.dest_port) {
            socket_fd = i;
            break;
        }
    }
    
    if (socket_fd == -1) {
        printf("Socket non trouvé pour le port %d\n", pdu.header.dest_port);
        return;
    }
    if (pdu.header.seq_num == expected_seq_num) {
        // Ajouter la donnée au buffer de réception
        app_buffer_put(pdu.payload);
        // Màj le prochain num de sequence attendu
        expected_seq_num = (expected_seq_num + 1) %2;
        printf("seq_num = %d\n", expected_seq_num);
    }

    // Construire et envoyer un ACK
    mic_tcp_pdu pdu_ack;
    pdu_ack.payload.size = 0;
    pdu_ack.header.source_port = pdu.header.dest_port;
    pdu_ack.header.dest_port = pdu.header.source_port;
    pdu_ack.header.ack_num = pdu.header.seq_num;
    pdu_ack.header.ack = 1;
    pdu_ack.header.syn = 0;
    printf("ack_num = %d\n", pdu_ack.header.ack_num);

    IP_send(pdu_ack, remote_addr);
}

