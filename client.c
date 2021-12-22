#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <regex.h>
#include <sys/time.h>

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;
extern int errno;
int quizz_count = 0;
bool timeout = false;

struct
{
    SA_IN address;
    int socket;
    bool isQuit;

} client;

void signalHandler()
{
    printf("\nInvalid input, try again...\n");
    printf("Your answer: ");
    fflush(stdout);
}

int check(int err, const char *msg)
{
    if (err < 0)
        printf("Error: %s (%d)\n", msg, errno), exit(0);
    return err;
}

void setupClient(const char *addr, int port)
{
    client.address.sin_family = AF_INET;
    client.address.sin_addr.s_addr = inet_addr(addr);
    client.address.sin_port = htons(port);
}

void connectClient()
{
    char nickname[256];

    printf("Enter a nickname: ");
    fflush(stdout);

    scanf("%s", nickname);

    check(client.socket = socket(AF_INET, SOCK_STREAM, 0),
          "Failed to create socket");
    check(connect(client.socket, (SA *)&client.address, sizeof(client.address)),
          "Failed to connect");

    check(write(client.socket, nickname, 256),
          "Failed writing to server");

    check(read(client.socket, &quizz_count, sizeof(int)),
          "Failed to read from server");

    client.isQuit = false;
    printf("Welcome, %s.\n", nickname);
}

void getAnswer(int q_time)
{
    char chosen_answer[256];
    int chosen_answer_converted;
    int exit = -1;
    int timeout = 0;

GET_INPUT:
    printf("Your answer: ");
    fflush(stdout);
    scanf("%s", chosen_answer);

    if (strcmp(chosen_answer, "exit") == 0)
    {
        check(write(client.socket, &exit, sizeof(int)),
              "Failed to write answer");
        check(read(client.socket, &timeout, sizeof(int)),
              "Failed to read from server");
        if (timeout == 1)
        {
            printf("\n####################\n");
            printf("Time expired! Your answer is not counted.");
        }
        else
        {
            client.isQuit = true;
        }
    }
    else
    {
        if (strlen(chosen_answer) > 1)
        {
            printf("Invalid input, try again...\n");
            goto GET_INPUT;
        }
        else
        {
            chosen_answer_converted = atoi(&chosen_answer[0]);
        }
        if (chosen_answer_converted >= 1 && chosen_answer_converted <= quizz_count)
        {
            check(write(client.socket, &chosen_answer_converted, sizeof(int)),
                  "Failed to write answer");
            check(read(client.socket, &timeout, sizeof(int)),
                  "Failed to read from server");
            if (timeout == 1)
            {
                printf("\n####################\n");
                printf("Time expired! Your answer is not counted.");
            }
        }
        else
        {
            printf("Invalid input, try again...\n");
            goto GET_INPUT;
        }
    }
}

void playGame()
{
    if (client.isQuit == true)
        return;

    char question[1024];
    char time[1024];
    char answers[1024];

    check(read(client.socket, question, 1024),
          "Failed to read question");
    check(read(client.socket, time, 1024),
          "Failed to read time");
    check(read(client.socket, answers, 1024),
          "Failed to read answers");

    printf("\n####################\n\n");
    printf("%s\n", question);
    printf("%s\n", time);
    printf("%s\n", answers);

    int q_time = atoi(&time[6]);

    getAnswer(q_time);
}

void outputRanking()
{
    char ranking[4096];

    check(read(client.socket, ranking, 4096),
          "Failed to read from server");

    printf("%s\n", ranking);

    close(client.socket);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Invaild request. Please enter server address and port...\n");
        return -1;
    }

    int port = atoi(argv[2]);
    const char *addr = argv[1];

    setupClient(addr, port);
    connectClient();

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGQUIT, signalHandler);
    signal(SIGTSTP, signalHandler);

    int q_id = 0;

    while (1)
    {
        if (client.isQuit == true)
        {
            printf("You left the game.\n");
            break;
        }
        else if (q_id == quizz_count)
        {
            char msg[1024];
            check(read(client.socket, msg, 1024),
                  "Failed to read from server");
            printf("%s", msg);
            outputRanking();
            break;
        }
        else
        {
            q_id++;
            playGame();
        }
    }
}
