#include <mictcp.h>
#include <api/mictcp_core.h>


//Tableau contenant les mic_tcp_sock référencés par leur fd
#define MAX_SOCKET 100
mic_tcp_sock tab[MAX_SOCKET];

mic_tcp_sock *get_socket(int fd){
    if (fd < 0 || fd >= MAX_SOCKET){
        return NULL;
    }
    return &tab[fd];
}


/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
   set_loss_rate(0);
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
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
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
    s->remote_addr=addr;
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
    mic_tcp_pdu pdu;
    pdu.header.source_port = s->local_addr.port;
    pdu.header.dest_port = s->remote_addr.port;
    pdu.payload.data = mesg;
    pdu.payload.size = mesg_size;

    //Envoyer le PDU
    int effective_send = IP_send(pdu, s->remote_addr.ip_addr);

    //Attendre ACK
    mic_tcp_pdu pdu_ack;
    unsigned long timeout = 1000;
    IP_recv(&pdu_ack, &s->local_addr, &s->remote_addr, timeout);
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

    // Ajouter la donnée au buffer de réception
    app_buffer_put(pdu.payload);

    // Construire et envoyer un ACK
    mic_tcp_pdu pdu_ack;
    pdu_ack.payload.size = 0;
    pdu_ack.header.source_port = pdu.header.dest_port;
    pdu_ack.header.dest_port = pdu.header.source_port;
    pdu_ack.header.ack_num = pdu.header.seq_num;
    pdu_ack.header.ack = 1;

    IP_send(pdu_ack, remote_addr);
}
