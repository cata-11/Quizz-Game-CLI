#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sqlite3.h>
#include "database.h"

#define PORT 5001
#define BACKLOG 1

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;
extern int errno;

typedef struct client_th
{
    int socket;
    bool isClosed;
    SA_IN address;
    char name[256];
    long long int id;
} client_th;

struct
{
    SA_IN address;
    int socket;
} server;

struct data
{
    char name[256];
    int points;
    bool isDone;
    bool isDisqualified;
    int socket;
} clients[1000];

struct
{
    long long int players_count;
    long long int players_done;
    long long int active_players;
    char ranking[4096];
} game;

int check(int err, const char *msg)
{
    if (err < 0)
        printf("Error: %s (%d)\n", msg, errno);
    return err;
}

void setupServer()
{
    bzero(&server.address, sizeof(server.address));
    server.address.sin_family = AF_INET;
    server.address.sin_port = htons(PORT);
    server.address.sin_addr.s_addr = INADDR_ANY;
}

void createSocket()
{
    check(server.socket = socket(AF_INET, SOCK_STREAM, 0),
          "Failed to create socket");
    check(setsockopt(server.socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &(int){1}, sizeof(int)),
          "Failed to reuse address");
    check(bind(server.socket, (SA *)&server.address, sizeof(server.address)),
          "Failed to bind");
    check(listen(server.socket, BACKLOG),
          "Failed to listen");
}

void sendData(void *arg)
{
    client_th *th_arg = (client_th *)arg;
    int socket = th_arg->socket;

    check(write(socket, quizz.data, 1024),
          "Failed to write to client 1");
}

void extractData(int id, void *arg)
{
    extractQuestion(id);
    sendData(arg);
    extractTimer(id);
    sendData(arg);
    extractAnswers(id);
    sendData(arg);
}

void checkAnswer(int q_id, int ans, void *arg)
{
    client_th *th_arg = (client_th *)arg;

    if (ans == quizz.correct_answers[q_id])
    {
        clients[th_arg->id].points += 20;
    }

    if (q_id == 5)
    {
        game.players_done++;
        clients[th_arg->id].isDone = true;
    }
}

void exitClient(void *arg)
{
    client_th *th_arg = (client_th *)arg;

    th_arg->isClosed = true;
    clients[th_arg->id].isDone = true;
    clients[th_arg->id].isDisqualified = true;
    clients[th_arg->id].points = -1;
    game.active_players--;

    printf("%s left the game (TID: %ld)\n", th_arg->name, pthread_self());

    shutdown(clients[th_arg->id].socket, SHUT_RDWR);
    close(clients[th_arg->id].socket);
}

void getAnswer(int id, void *arg)
{
    client_th *th_arg = (client_th *)arg;

    int chosen_answer;
    int socket = th_arg->socket;

    struct timeval timer;
    timer.tv_sec = quizz.timer;
    int timeout_msg = 0;

    fd_set active_sockets;
    FD_ZERO(&active_sockets);
    FD_SET(clients[th_arg->id].socket, &active_sockets);

    int rt;

    rt = check(select(clients[th_arg->id].socket + 1, &active_sockets, NULL, NULL, &timer),
               "Failed to select answer");

    if (rt == 0)
    {
        printf("%s didn't answer to Q%d (timeout) (TID: %ld)\n",
               th_arg->name, id, pthread_self());

        timeout_msg = 1;
        check(read(socket, &chosen_answer, sizeof(int)),
              "Failed to read from client 1");

        chosen_answer = 0;
        check(write(socket, &timeout_msg, sizeof(int)),
              "Failed to write to client 2");
    }
    else
    {
        check(read(socket, &chosen_answer, sizeof(int)),
              "Failed to read from client 2");
        check(write(socket, &timeout_msg, sizeof(int)),
              "Failed to write to client 3");

        if (chosen_answer == -1)
        {
            exitClient(arg);
            return;
        }
        else
        {
            printf("%s answers to Q%d with %d (TID: %ld)\n",
                   th_arg->name, id, chosen_answer, pthread_self());
        }
    }
    checkAnswer(id, chosen_answer, arg);
}

void playGame(void *arg)
{
    client_th *th_arg = (client_th *)arg;
    int socket = th_arg->socket;
    bool isDone = clients[th_arg->id].isDone;

    for (int id = 1; id <= quizz.count; ++id)
    {
        if (th_arg->isClosed == true)
            break;
        extractData(id, arg);
        getAnswer(id, arg);
    }
    if (th_arg->isClosed == true)
    {
        return;
    }
    else
    {
        printf("%s finished the quizz with %d points (TID: %ld)\n",
               th_arg->name, clients[th_arg->id].points, pthread_self());

        char msg[1024];

        sprintf(msg, "\n########################################\n\nYou finished the quizz with %d points\n\nWaiting for other players to finish...\n", clients[th_arg->id].points);

        check(write(socket, msg, 1024),
              "Failed to write to client 4");
    }
}

void makeRanking()
{
    struct data aux;
    char temp[4096];

    sprintf(temp, "\n########################################\n\nRanking:\n\n");
    strcpy(game.ranking, temp);

    for (int i = 0; i < game.players_count - 1; ++i)
    {
        for (int j = i + 1; j < game.players_count; ++j)
        {
            if (clients[i].points < clients[j].points)
            {
                aux = clients[j];
                clients[j] = clients[i];
                clients[i] = aux;
            }
        }
    }

    for (int i = 0; i < game.players_count; ++i)
    {
        if (clients[i].isDisqualified)
        {
            sprintf(temp, "[disqualified]: %s\n", clients[i].name);
        }
        else
        {
            sprintf(temp, "[rank %d]: %s (%d points)\n", i + 1, clients[i].name, clients[i].points);
        }
        strcat(game.ranking, temp);
    }
}

void sendRanking(int i)
{
    if (clients[i].isDisqualified == true)
        return;
    check(write(clients[i].socket, game.ranking, 4096),
          "Failed to write to clients");
    close(clients[i].socket);
}

void clearData()
{
    for (int i = 0; i < game.players_count; ++i)
    {
        clients[i].points = 0;
        close(clients[i].socket);
    }
    game.players_count = 0;
    game.players_done = 0;
    game.active_players = 0;
    memset(game.ranking, 0, sizeof(game.ranking));

    printf("\nNew game session has started\n\nWaiting for connections...\n\n");
}

void *threadRoutine(void *arg)
{
    client_th *th_arg = (client_th *)arg;

    SA_IN address = th_arg->address;
    int socket = th_arg->socket;
    char nickname[256];

    check(read(socket, nickname, 256),
          "Failed to read from client 3");
    check(write(socket, &quizz.count, sizeof(int)),
          "Failed to write to client 6");
    strcpy(th_arg->name, nickname);
    strcpy(clients[th_arg->id].name, nickname);
    clients[th_arg->id].isDone = false;
    clients[th_arg->id].isDisqualified = false;

    printf("%s joined the game (TID: %ld)\n",
           th_arg->name, pthread_self());

    playGame(arg);

    close((intptr_t)arg);

    if (game.active_players == game.players_done)
    {
        printf("\nGame session is over!\n");

        makeRanking();

        for (int i = 0; i < game.players_count; ++i)
        {
            sendRanking(i);
        }

        clearData();
    }
    return (NULL);
}

void connectClient()
{
    client_th *th_arg;

    socklen_t addr_length;
    pthread_t thread;
    int socket;

    if ((th_arg = ((client_th *)malloc(sizeof *th_arg))) == NULL)
    {
        printf("Error allocating memory");
    }

    addr_length = sizeof(th_arg->address);

    socket = check(accept(server.socket, (SA *)&th_arg->address, &addr_length),
                   "Failed to accept client");

    th_arg->socket = socket;
    th_arg->id = game.players_count++;
    game.active_players++;
    th_arg->isClosed = false;
    clients[th_arg->id].socket = socket;

    if (pthread_create(&thread, NULL, threadRoutine, (void *)th_arg) != 0)
    {
        printf("Error creating thread");
    }
}

void initApp()
{
    createDatabase();
    extractCorrectAnswers();
}

int main()
{
    initApp();

    setupServer();
    createSocket();

    printf("Waiting for connections...\n\n");

    while (1)
    {
        connectClient();
    }

    return 0;
}
