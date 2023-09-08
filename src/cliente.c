/**********************************************************************
 * CLIENTE
 * USO: >cliente <enderecoServidor>  <porto>
 **********************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/wait.h>

#define UDP_PORT 160
#define TCP_PORT 80

#define BUF_SIZE 1000
#define MAXCOMANDSIZE 25
#define MAXLINESIZE 250

int shmid;
typedef struct{
    char ip[MAXCOMANDSIZE];
    int porta;
}shm;
shm *sharedMemory;

int fd;
pid_t pid;

void sigint(int s);
void terminar();
void erro(char *msg);
int lerConsola(char message[], int size);
int lerLinhaConsola(char message[], int size);
void lerUserPassword(char message[], char user[]);
void lerAdmin(char message[]);
void receberMensagens(int fd, struct sockaddr_in addr, socklen_t slen);
void guardarAutorizacoes(int autorizacoes[], char message[]);
void split(char *array[], char string[]);
int myAtoi(char s[]);

void cliente();
void admin();


int main(int argc, char *argv[]) {
    pid = getpid();
    signal(SIGINT, sigint);
    signal(SIGUSR1, terminar);
    if(argc != 3) {
        printf("cliente {endereco do servidor} {porto}\n");
        exit(-1);
    }

    struct hostent *hostPtr;
    char endServer[100];
    strcpy(endServer, argv[1]);
    if ((hostPtr = gethostbyname(endServer)) == 0)
        erro("Não consegui obter endereço");
    int server_port = myAtoi(argv[2]);

    struct sockaddr_in addr;
    bzero((void *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
    addr.sin_port = htons((short) atoi(argv[2]));
    socklen_t slen = sizeof(addr);

    if (server_port == UDP_PORT) {
        if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
            erro("socket");

        cliente(fd, addr, slen);
    }
    else if (server_port == TCP_PORT) {
        if ((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
            erro("socket");
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            erro("Connect");

        admin();
    }
}

void cliente(int fd, struct sockaddr_in addr, socklen_t slen) {
    printf("\n---------- CLIENTE ----------\n");

    if ((shmid = shmget(IPC_PRIVATE, sizeof(shm), IPC_CREAT|0777)) < 0) {
        erro("Error in shmget");
        exit(1);
    }

    if ((sharedMemory = (shm*) shmat(shmid, NULL, 0)) == (shm*) -1) {
        erro("Error in shmat");
        exit(1);
    }
    sharedMemory->porta = -1;
    strcpy(sharedMemory->ip, "");

    char username[MAXCOMANDSIZE];
    char message[2*MAXCOMANDSIZE+1];
    lerUserPassword(message, username);

    char comando[BUF_SIZE];
    sprintf(comando, "LOGIN|%s", message);

    sendto(fd, comando, 1 + strlen(comando), 0, (struct sockaddr *) &addr, slen);

    char info[MAXLINESIZE];
    int recv_len;
    if((recv_len = recvfrom(fd, info, MAXLINESIZE, 0, (struct sockaddr *) &addr, (socklen_t *)&slen)) == -1) {
	  erro("Erro no recvfrom");
	}
    info[recv_len] = '\0';

    printf("%s\n", info);
    char menu[BUF_SIZE];
    strncpy(menu, info+15, strlen(info)-1);

    if (strcmp(info, "User não existente") != 0) {
        int autorizacoes[3] = {0, 0, 0};
        guardarAutorizacoes(autorizacoes, info);

        if (fork() == 0) {
            receberMensagens(fd, addr, slen);
        }

        char opcao[MAXCOMANDSIZE];
        do {
            char s[MAXLINESIZE];
            char temp[MAXLINESIZE+500];
            char s1[BUF_SIZE];
            char s2[BUF_SIZE+500];
            lerConsola(opcao, MAXCOMANDSIZE);

            if (strcmp(opcao, "Cliente-Servidor") == 0 && autorizacoes[0] == 1) {
                printf("Indique a mensagem, assim como o user-id de destino (mensagem*user-id)\n");
                lerLinhaConsola(s, MAXLINESIZE);
                sprintf(temp,"CS|####################\nNova mensagem recebida pelo servidor do user %s:\n   %s", username, s);
                sendto(fd, temp, 1 + strlen(temp), 0, (struct sockaddr *) &addr, slen);
            }
            else if (strcmp(opcao, "P2P") == 0 && autorizacoes[1] == 1) {
                printf("Indique o user-id de destino\n");
                lerConsola(s, MAXLINESIZE);
                sprintf(temp,"P2P|%s",s);
                sendto(fd, temp, 1 + strlen(temp), 0, (struct sockaddr *) &addr, slen);

                while(sharedMemory->porta == -1 && strlen(sharedMemory->ip) == 0);
                struct sockaddr_in socketPeer2Peer;

                socketPeer2Peer.sin_family = AF_INET;
                socketPeer2Peer.sin_port = htons(sharedMemory->porta);
                socketPeer2Peer.sin_addr.s_addr = inet_addr(sharedMemory->ip);

                printf("Indique a mensagem\n");
                lerLinhaConsola(s1, BUF_SIZE);
                sprintf(s2, "####################\nNova mensagem recebida do user %s:\n   %s\n####################", username, s1);

                printf("%s %d <%s>\n", sharedMemory->ip, sharedMemory->porta, s1);
                sendto(fd, s2, BUF_SIZE+500, 0, (struct sockaddr *) &socketPeer2Peer, sizeof(socketPeer2Peer));
                sharedMemory->porta = -1;
                strcpy(sharedMemory->ip,"");
            }
            else if (strcmp(opcao, "Grupo") == 0 && autorizacoes[2] == 1) {
                sendto(fd, temp, 1 + strlen(temp), 0, (struct sockaddr *) &addr, slen);
            }
            if (strcmp(opcao, "Sair") != 0)
                printf("%s\n", menu);
        } while (strcmp(opcao, "Sair") != 0);
    }

    exit(0);
}

void admin(int fd, struct sockaddr_in addr, socklen_t slen) {
    char message[2*MAXCOMANDSIZE+1];
    lerAdmin(message);
    write(fd, message, strlen(message)+1);
    int nread;
    char option[MAXLINESIZE];
    char buffer[BUF_SIZE];
    do{
        nread = read(fd, buffer, BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s",buffer);

        lerLinhaConsola(option,MAXLINESIZE);
        write(fd,option,1+strlen(option));

        nread = read(fd,buffer, BUF_SIZE-1);
        buffer[nread] = '\0';
        printf("%s", buffer);
    } while(strcmp(option,"QUIT")!=0);

    write(fd, option, 10);
    fflush(stdout);
}



////////////////////////////////////////////////////////////////////////////////

void erro(char *msg) {
    printf("Erro: %s\n", msg);
	exit(-1);
}

void terminar() {
    if (getpid() == pid) {
        printf("\nClosing\n");

        close(fd);

        if (shmid >= 0)
            shmctl(shmid, IPC_RMID, NULL);

        for(int i = 0; i < 2; i++)
            wait(NULL);
    }
    exit(0);
}

void sigint(int s) {
    if (s == SIGINT) {
        terminar();
    }
}

void receberMensagens(int fd, struct sockaddr_in addr, socklen_t slen) {
    int recv_len;
    char msg[BUF_SIZE];
    while(1) {
        //printf("ESPERA\n");
        if ((recv_len = recvfrom(fd, msg, BUF_SIZE, 0, (struct sockaddr *) &addr, (socklen_t *)&slen)) == -1) {
    	  erro("Erro no recvfrom");
    	}
        msg[recv_len] = '\0';

        if (strcmp(msg, "ERROR") == 0) {
            printf("User não existente/online\n");
            raise(SIGUSR1);
        }
        else {
        char temp[strlen(msg)];
        strcpy(temp,msg);
        char *comando[BUF_SIZE];
        int aux = 0;
        char *elem = strtok(temp, "| ");
        if (strcmp(elem, "P2P") == 0) {
            while(elem != NULL) {
                //printf("   %s\n", elem);
                comando[aux++] = elem;
                elem = strtok(NULL, "| ");
            }
            sharedMemory->porta = myAtoi(comando[1]);
            strcpy(sharedMemory->ip, comando[2]);
        }

        printf("%s\n", msg);
        }
        //memset(msg,0,sizeof(msg));
        fflush(stdout);
    }
}

int lerConsola(char message[], int size) {
    fgets(message, size, stdin);
    if (strlen(message) > 0) {
        message[strlen(message)-1] = '\0';

        for (int i=0; i<strlen(message); i++) {
            if (message[i] == ' ') {
                printf("Erro, comando incorreto\n");
                return 1;
            }
        }

        return 0;
    }
    else {
        printf("Erro ao ler da consola\n");
        return 1;
    }
}

int lerLinhaConsola(char message[], int size) {
    fgets(message, size, stdin);
    if (strlen(message) > 0) {
        message[strlen(message)-1] = '\0';

        return 0;
    }
    else {
        printf("Erro ao ler da consola\n");
        return 1;
    }
}

void lerUserPassword(char message[], char user[]) {
    //char user[MAXCOMANDSIZE];
    do {
        printf("Introduza o nome de utilizador: ");
    } while (lerConsola(user, MAXCOMANDSIZE) != 0);
    //printf("%s\n", user);

    char password[MAXCOMANDSIZE];
    do {
        printf("Introduza a password: ");
    } while (lerConsola(password, MAXCOMANDSIZE) != 0);
    //printf("%s\n", password);

    sprintf(message, "%s,%s", user, password);
    //printf("%s\n", message);
}

void lerAdmin(char message[]) {
    char user[MAXCOMANDSIZE];
    do {
        printf("Introduza o nome de utilizador: ");
    } while (lerConsola(user, MAXCOMANDSIZE) != 0);
    //printf("%s\n", user);

    char password[MAXCOMANDSIZE];
    do {
        printf("Introduza a password: ");
    } while (lerConsola(password, MAXCOMANDSIZE) != 0);
    //printf("%s\n", password);

    sprintf(message, "%s,%s", user, password);
    //printf("3%s\n", message);
}

void guardarAutorizacoes(int autorizacoes[3], char message[]) {
    char t[MAXLINESIZE];
    strcpy(t, message);
    char *temp[7];
    int aux = 0;
    char *elem = strtok(t, "\n");
    while(elem != NULL) {
        temp[aux++] = elem;
        elem = strtok(NULL, "\n");
    }

    for (int i=2; i<aux-2; i++) {
        if (strcmp(temp[i], "     Cliente-Servidor") == 0)
            autorizacoes[0] = 1;
        else if (strcmp(temp[i], "     P2P") == 0)
            autorizacoes[1] = 1;
        else if (strcmp(temp[i], "     Grupo") == 0)
            autorizacoes[2] = 1;
    }
}

void split(char *array[], char string[]) {
    int aux = 0;
    char *elem = strtok(string, ",");
    while(elem != NULL) {
        array[aux++] = elem;
        elem = strtok(NULL, ",");
    }
}

int myAtoi(char s[]) {
    int erro = 0;
    int res = 0;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i]>='0' && s[i]<='9') {
            res = res * 10 + s[i] - '0';
        }
        else {
            erro = -1;
            break;
        }
    }

    if (erro == 0)
        return res;
    else
        return erro;
}
