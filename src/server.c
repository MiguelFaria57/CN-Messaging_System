/*******************************************************************************
 * SERVIDOR
 * USO: >server <porto clientes>  <porto config>  <ficheiro de registos>
 *******************************************************************************/
#include <sys/socket.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <errno.h>

#define BUF_SIZE 1000
#define MAXLINESIZE 250
#define MAXCOMANDSIZE 25
#define MAXUSERS 100

int myAtoi(char s[]);
int confirmData(int porta, char portas[50][3][MAXCOMANDSIZE], char *message, char ip_a[], char registos[6][MAXCOMANDSIZE], char filename[]);
void split(char *array[MAXCOMANDSIZE], char string[]);
void lerFicheiro(char fich[], char filename[]);
int checkIP(char separa[MAXCOMANDSIZE]);
int checkUser(char *filename,char *info);
void splitEspaco(char *array[MAXCOMANDSIZE], char texto[]);
int checkAdmin(char *filename, char *linha, char clientAddress[MAXLINESIZE]);
void erro(char *msg);
void sigint(int s);
void udpFunction(int server_port_udp,char *filename);
void comandoLogin(int fd_udp, struct sockaddr_in si_outra, int slen, char filename[], char buf[], char portas[50][3][MAXCOMANDSIZE]);
void clientServer(int fd, char *message, char portas[50][3][MAXCOMANDSIZE]);
void peer2peer(int fd_udp, struct sockaddr_in si_outra, char *message, char portas[50][3][MAXCOMANDSIZE]);
int checkStep(char msg[], char s[]);
void tcpFunction(int server_port_tcp, char *filename);
void admin(int fd, char *filename, char clientAddress[MAXLINESIZE], int clientPort);
int addUser(int fd, char *separado[],char *filename);
void deleteUser(char *filename, int linha);



pid_t pid;
int numberClient = 0;
int client, fd_tcp, fd_udp;



int main(int argc, char *argv[]) {
    pid = getpid();
    signal(SIGINT, sigint);

    if(argc != 4){
    	printf("server {porto clientes} {porto config} {ficheiro de registos}\n");
    	exit(-1);
    }
    int server_port_udp = myAtoi(argv[1]);
    int server_port_tcp = myAtoi(argv[2]);
    char filename[MAXLINESIZE];
    strcpy(filename,argv[3]);

    printf("\n---------- SERVER ----------\n");
    if (fork() == 0) {
    	tcpFunction(server_port_tcp, filename);
    }
    else {
        udpFunction(server_port_udp, filename);
    }

    return 1;
}


////////////////////////////////////////////////////////////////////////////////
// UDP

void udpFunction(int server_port_udp, char *filename) {
	struct sockaddr_in si_minha, si_outra;
    socklen_t slen = sizeof(si_outra);

	if((fd_udp=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		erro("Erro na criação do socket");
	}

    bzero((void *) &si_minha, sizeof(si_minha));

	si_minha.sin_family = AF_INET;
	si_minha.sin_port = htons(server_port_udp);
	si_minha.sin_addr.s_addr = inet_addr("10.90.0.2");

	if(bind(fd_udp, (struct sockaddr*)&si_minha, sizeof(si_minha)) == -1) {
		erro("Erro no bind");
	}

    char portas[50][3][MAXCOMANDSIZE];

    char buf[BUF_SIZE];
	while(1) {
        int recv_len;
		if((recv_len = recvfrom(fd_udp, buf, BUF_SIZE, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1) {
			erro("Erro no recvfrom (server)");
		}
        buf[recv_len]='\0';

        char string[MAXLINESIZE];
        int funcao = checkStep(buf, string);

        if (funcao == 1)
            comandoLogin(fd_udp, si_outra, slen, filename, string, portas);
        if (funcao == 2)
            clientServer(fd_udp, string, portas);
        if (funcao == 3)
            peer2peer(fd_udp, si_outra, string, portas);
    }

    close(fd_udp);
}

int checkStep(char msg[], char s[]) {
    char *comando[BUF_SIZE];
    int aux = 0;
    char *elem = strtok(msg, "|");
    while(elem != NULL) {
        comando[aux++] = elem;
        elem = strtok(NULL, "|");
    }
    strcpy(s, comando[1]);

    if (strcmp(comando[0],"LOGIN") == 0) {
        return 1;
    }
    if (strcmp(comando[0], "CS") == 0) {
        return 2;
    }
    if (strcmp(comando[0], "P2P") == 0) {
        return 3;
    }
    else {
        return 0;
    }
}

void comandoLogin(int fd_udp, struct sockaddr_in si_outra, int slen, char filename[], char buf[], char portas[50][3][MAXCOMANDSIZE]) {
    char clientAddress[MAXLINESIZE];
    int clientPort;
    char registoUser[6][MAXCOMANDSIZE];

    strcpy(clientAddress,inet_ntoa(si_outra.sin_addr));
    clientPort = ntohs(si_outra.sin_port);

    int check = confirmData(clientPort, portas, buf, clientAddress, registoUser, filename);
    if(check != 1){
        strcpy(buf, "User não existente");
        sendto(fd_udp, buf, BUF_SIZE, 0, (struct sockaddr *) &si_outra, slen);
    }
    else {
        printf("User conectado com o endereço %s e o porto %d\n", clientAddress, clientPort);

        char menu[BUF_SIZE] = "\nUser conectado\nEstá autorizado a realizar os seguintes tipos de comunicação:\n";
        if (strcmp(registoUser[3], "yes") == 0)
            strcat(menu, "     Cliente-Servidor\n");
        if (strcmp(registoUser[4], "yes") == 0)
            strcat(menu, "     P2P\n");
        if (strcmp(registoUser[5], "yes\n") == 0)
            strcat(menu, "     Grupo\n");
        strcat(menu, "     Sair\nEscolha a comunicação que pretende: ");

        sendto(fd_udp, menu, BUF_SIZE, 0, (struct sockaddr *) &si_outra, slen);
    }
}

void clientServer(int fd, char *message, char portas[50][3][MAXCOMANDSIZE]) {
    char *array[2];
    int aux = 0;
    char *elem = strtok(message, "*");
    while(elem != NULL) {
        array[aux++] = elem;
        elem = strtok(NULL, "*");
    }
    char ip[MAXCOMANDSIZE];
    int porta;
    int check = 0;
    for(int i = 0; i < 50; i++){
        if(strcmp(portas[i][0],array[1])==0){
            porta = myAtoi(portas[i][1]);
            strcpy(ip,portas[i][2]);
            check = 1;
            break;
        }
    }
    if(check == 1){
        struct sockaddr_in socketClientServer;

        socketClientServer.sin_family = AF_INET;
        socketClientServer.sin_port = htons(porta);
        socketClientServer.sin_addr.s_addr = inet_addr(ip);

        printf("%s %d \n%s\n%s\n", ip, porta, array[0], array[1]);

        char texto[BUF_SIZE+500];
        sprintf(texto, "%s\n####################", array[0]);
        sendto(fd, texto, BUF_SIZE+500, 0, (struct sockaddr *) &socketClientServer, sizeof(socketClientServer));
    }
}

void peer2peer(int fd_udp, struct sockaddr_in si_outra, char *message, char portas[50][3][MAXCOMANDSIZE]) {
    char mensagem[MAXLINESIZE];
    int check = 0;
    for(int i = 0; i < 50; i++) {
        if(strcmp(portas[i][0], message) == 0){
            sprintf(mensagem,"P2P|%s %s",portas[i][1],portas[i][2]);
            check = 1;
            break;
        }
    }
    if(check == 1) {
        sendto(fd_udp, mensagem, BUF_SIZE, 0, (struct sockaddr *) &si_outra, sizeof(si_outra));
    }
    else {
        sprintf(mensagem,"ERROR");
        sendto(fd_udp, mensagem, BUF_SIZE, 0, (struct sockaddr *) &si_outra, sizeof(si_outra));

    }
}

////////////////////////////////////////////////////////////////////////////////
// TCP

void tcpFunction(int server_port_tcp, char *filename) {
    struct sockaddr_in addr, client_addr;
    int client_addr_size;

    bzero((void *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("10.90.0.2");
    addr.sin_port = htons(server_port_tcp);

    if ((fd_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        erro("na funcao socket");
    if (setsockopt(fd_tcp, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        erro("na funcao setsocketopt");
    if (bind(fd_tcp,(struct sockaddr *)&addr,sizeof(addr)) < 0)
        erro("na funcao bind");
    if (listen(fd_tcp, 5) < 0)
        erro("na funcao listen");
    client_addr_size = sizeof(client_addr);

    char IPbuffer[100];
    if (inet_ntop(AF_INET,&(addr.sin_addr.s_addr),IPbuffer,INET_ADDRSTRLEN)<0)
        erro("na funcao ntop server");

    while (1) {
        client = accept(fd_tcp,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
        char clientAddress[MAXLINESIZE];
        int clientPort;
        strcpy(clientAddress,inet_ntoa(client_addr.sin_addr));
        clientPort = ntohs(client_addr.sin_port);

        if (client > 0) {
            admin(client, filename, clientAddress, clientPort);
            close(client);
            exit(0);
        }
    }
}


void admin(int fd, char *filename, char clientAddress[MAXLINESIZE], int clientPort) {
    int nread = 0;
    char buffer[BUF_SIZE];
    char message[BUF_SIZE];
    nread = read(fd, buffer, BUF_SIZE);
    buffer[nread] = '\0';
    printf("3 - %s\n", buffer);
    if (checkAdmin(filename, buffer, clientAddress) == 1) {
        printf("Admin conectado com o endereço %s e o porto %d\n", clientAddress, clientPort);
        do {
            char menu[BUF_SIZE] = "\nQue comando deseja realizar?:\n    LIST\n    ADD <User-id> <IP> <Password> <Cliente-Servidor> <P2P> <Grupo>\n    DEL <User-id>\n    QUIT\n (escreva o nome do comando)\n";
            write(client, menu, strlen(menu) + 1);

            strcpy(buffer, "\0");
            nread = read(fd, buffer, BUF_SIZE - 1);
            buffer[nread] = '\0';

            char *separado[MAXLINESIZE];
            //splitEspaco(separado,buffer);
            char string[BUF_SIZE];
            strcpy(string, buffer);
            int aux = 0;
            char *elem = strtok(string, " ");
            while(elem != NULL) {
                separado[aux++] = elem;
                elem = strtok(NULL, " ");
            }

            if (strcmp(separado[0], "LIST")==0) {
                strcpy(buffer, "\0");
                lerFicheiro(buffer,filename);
                strcpy(message, buffer);
            }
            else if(strcmp(separado[0], "ADD")==0){
                char t[BUF_SIZE];
                int i;
                for (i = 0; i<strlen(buffer); i++) {
                    if (i >= 4) {
                        if (buffer[i] == ' ')
                            t[i-4] = ',';
                        else
                            t[i-4] = buffer[i];
                    }
                }
                t[i-4] = '\n';
                FILE *f = fopen(filename, "a");
                fputs(t, f);
                fclose(f);
                sprintf(message,"User inserido com sucesso.\n");
                /*strcpy(buffer, "\0");
                if(checkUser(filename, separado[1]) != 0){
                    sprintf(message,"Erro, username já existe");
                }
                else{
                    if(addUser(fd, separado,filename)==0)
                        sprintf(message,"Não foi possível inserir o user fornecido.\n");
                    else
                        sprintf(message,"User inserido com sucesso.\n");
                }*/
            }
            else if(strcmp(separado[0],"DEL")==0){
                strcpy(buffer, "\0");
                int linha = checkUser(filename, separado[1]);
                if(linha != 0) {
                    deleteUser(filename, linha);
                    strcpy(message, "User eliminado com sucesso.\n");
                }
                else
                    sprintf(message,"User não encontrado.\n");

            }
            else if(strcmp(separado[0],"QUIT")!=0){
                strcpy(buffer, "\0");
                sprintf(message,"Comando nao reconhecido.\n");
            }

            if (strcmp(buffer, "QUIT") != 0) {
                write(fd,message,strlen(message)-1);
            }

        } while (strcmp(buffer, "QUIT") != 0);
    }
}


////////////////////////////////////////////////////////////////////////////////

void sigint(int s) {
    if (s == SIGINT) {
        if (getpid() == pid) {
            printf("\nClosing\n");
            close(fd_tcp);
            close(fd_udp);
            close(client);

            for(int i = 0; i < 2; i++)
            	wait(NULL);
        }
        exit(0);
    }
}

void erro(char *msg){
    printf("Erro: %s\n", msg);
    exit(-1);
}


////////////////////////////////////////////////////////////////////////////////

void split(char *array[MAXCOMANDSIZE], char string[]) {
    int aux = 0;
    char *elem = strtok(string, ",");
    while(elem != NULL) {
        array[aux++] = elem;
        elem = strtok(NULL, ",");
    }
}

void splitEspaco(char *array[MAXCOMANDSIZE], char texto[]) {
    char string[strlen(texto)];
    strcpy(string, texto);
    int aux = 0;
    char *elem = strtok(string, " ");
    while(elem != NULL) {
        array[aux++] = elem;
        elem = strtok(NULL, " ");
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

int confirmData(int porta, char portas[50][3][MAXCOMANDSIZE], char *message, char ip_a[], char registos[6][MAXCOMANDSIZE], char filename[]) {

    char msg[strlen(message)];
    strcpy(msg, message);
    char *dados[2];
    split(dados, msg);


    FILE* fichRegistos = fopen(filename, "r");
    if (fichRegistos == NULL)
        printf("Erro ao abrir o ficheiro\n");

    int userExiste = 0;

    char ip[MAXLINESIZE];
    char line[MAXLINESIZE];
    while (fgets(line, MAXLINESIZE, fichRegistos) != NULL) {
        int x = strlen(line) -2;
        if (line[x] == '\n') {
            line[x]='\0';
        }

        char *r[6];
	    split(r, line);

        if (strcmp(dados[0], r[0]) == 0 && strcmp(ip_a, r[1]) == 0 && strcmp(dados[1], r[2]) == 0) {
            userExiste = 1;

            for (int i=0; i<6; i++) {
                strcpy(registos[i], r[i]);
            }

            strcpy(ip, r[1]);

            break;
        }
    }
    int i;
    int check = 0;
    if(userExiste == 1){
        for(i = 0; i < 50; i++){
            if(strlen(portas[i][0]) == 0) {
                break;
            }
            else if(strcmp(portas[i][0],dados[0])==0){
                check = 1;
                break;
            }
        }
        if(check != 1){
            strcpy(portas[i][0], dados[0]);
            sprintf(portas[i][1],"%d",porta);
            strcpy(portas[i][2],ip);
        }
    }
    printf("%s %s %s\n", portas[i][0], portas[i][1], portas[i][2]);
    fclose(fichRegistos);

    return userExiste;
}

void lerFicheiro(char fich[], char filename[]) {
    FILE* fichRegistos = fopen(filename, "r");
    if (fichRegistos == NULL)
        printf("Erro ao abrir o ficheiro\n");

    char line[MAXLINESIZE];
    while (fgets(line, MAXLINESIZE, fichRegistos) != NULL) {
        strcat(fich, line);
    }
    fclose(fichRegistos);
}

int checkIP(char separa[MAXCOMANDSIZE]) {
    char *elem = strtok(separa, ".");
    int check = 0;
    while(elem != NULL && check == 0) {
        if(myAtoi(elem)>=255 && myAtoi(elem)<=0)
            check = 1;
        elem = strtok(NULL, ".");
    }
    return check;
}

int checkUser(char *filename,char *info) {
    FILE* fichRegistos = fopen(filename, "r");
    if (fichRegistos == NULL)
        printf("Erro ao abrir o ficheiro\n");

    int userExiste = 0;

    char line[MAXLINESIZE];
    int nmrLinha = 0;
    while (fgets(line, MAXLINESIZE, fichRegistos) != NULL) {
        int x = strlen(line) -2;
        if (line[x] == '\n') {
            line[x]='\0';
        }

        nmrLinha++;
        char *r[6];
        split(r, line);
        if (strcmp(info, r[0]) == 0) {
            userExiste = nmrLinha;
            break;
        }
    }
    return userExiste;
}

int checkAdmin(char *filename, char *linha, char clientAddress[MAXLINESIZE]) {
    char *dados[MAXLINESIZE];
    split(dados, linha);

    FILE* fichRegistos = fopen(filename, "r");
    if (fichRegistos == NULL)
        printf("Erro ao abrir o ficheiro\n");

    char line[MAXLINESIZE];
    while (fgets(line, MAXLINESIZE, fichRegistos) != NULL) {
        int x = strlen(line) -2;
        if (line[x] == '\n') {
            line[x]='\0';
        }

        char *r[6];
	    split(r, line);

        if (strcmp(dados[0], r[0]) == 0 && strcmp(dados[1], r[2]) == 0 && strcmp(clientAddress, r[1]) == 0)
            return 1;
    }
    return 0;
}

int addUser(int fd, char *separado[], char *filename){
    int check = 0;
    check++;
    if(checkIP(separado[2]) == 0)
        check++;
    check++;
    if((strcmp(separado[3],"yes") == 0 ||strcmp(separado[3],"no") == 0) && (strcmp(separado[4],"yes") == 0 ||strcmp(separado[4],"no") == 0) && (strcmp(separado[5],"yes") == 0||strcmp(separado[5],"no") == 0))
        check++;
    if(check == 4){
        FILE* f = fopen(filename, "a");
        if (f == NULL)
            erro("Ao abrir o ficheiro\n");

        char temp[MAXLINESIZE];
        for(int i = 0; i<6;i++){
            if(i==0)
                strcpy(temp, separado[i]);
            else{
                sprintf(temp,",%s", separado[i]);
            }
        }
        printf("%s\n", temp);
        strcat(temp, "\n");
        fputs(temp, f);
        fclose(f);
        return 1;
    }
    else
        return 0;
}

void deleteUser(char *filename, int linha) {
    char str[MAXLINESIZE];

    char temp[MAXCOMANDSIZE] = "temporario.txt";
    FILE* f = fopen(filename,"r");
    FILE* f2 = fopen(temp, "w");
    int current = 0;
    while (!feof(f)){

        strcpy(str, "\0");
        fgets(str, MAXLINESIZE, f);
        if (!feof(f)){
            current++;

            if (current != linha){
                fprintf(f2, "%s", str);
            }
        }
    }
    fclose(f);
    fclose(f2);
    remove(filename);
    rename(temp, filename);
}
