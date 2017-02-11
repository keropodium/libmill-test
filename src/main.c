#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../deps/libmill/libmill.h"

#define SOCKET_SEND_END     "\r\n"
#define SOCKET_READ_END     "\r"
#define PIPE                "|"
#define NO_DEADLINE         -1
#define CONN_ESTABLISHED    1
#define CONN_SUCCEEDED      2
#define CONN_FAILED         3

void clearScreen() {
    printf("\033[2J");
    printf("\033[1;1H");
}

void putMessage(tcpsock connection, char message[], int deadline) {
    tcpsend(connection, message, strlen(message), deadline);
}

void sendMessages(tcpsock connection, int deadline) {
    putMessage(connection, SOCKET_SEND_END, deadline);
    tcpflush(connection, deadline);
}

void getMessage(tcpsock connection, char inbuf[], int deadline) {
    size_t sz = tcprecvuntil(connection, inbuf, sizeof(inbuf), SOCKET_READ_END, 1, deadline);
    inbuf[sz - 1] = 0;
}

void closeConnection(tcpsock connection, chan statisticsChannel) {
    if (errno == 0)
        chs(statisticsChannel, int, CONN_SUCCEEDED);
    else {
        chs(statisticsChannel, int, CONN_FAILED);
        if (errno == ETIMEDOUT) {
            putMessage(connection, "TIMEOUT", NO_DEADLINE);
            if (errno == 0)
                sendMessages(connection, NO_DEADLINE);
        }
    }
    tcpclose(connection);
}

int sumPipelinedTokens(char source[]) {
    int result = 0;
    char *token;

    token = strtok(source, PIPE);

    while (token != NULL) {
        result += atoi(token);
        token = strtok(NULL, PIPE);
    }

    return result;
}

coroutine void dialogue(tcpsock connection, chan statisticsChannel) {
    chs(statisticsChannel, int, CONN_ESTABLISHED);

    while (1) {
        int64_t const TWO_MINUTES_DEADLINE = now() + 120000;

        char inbuf[256];
        getMessage(connection, inbuf, TWO_MINUTES_DEADLINE);
        if (errno != 0)
            return closeConnection(connection, statisticsChannel);

        char outbuf[256];
        int rc = snprintf(outbuf, sizeof(outbuf), "%d", sumPipelinedTokens(inbuf));
        if (errno != 0)
            return closeConnection(connection, statisticsChannel);

        putMessage(connection, outbuf, NO_DEADLINE);
        if (errno != 0)
            return closeConnection(connection, statisticsChannel);
        sendMessages(connection, NO_DEADLINE);
        if (errno != 0)
            return closeConnection(connection, statisticsChannel);
    }
}

coroutine void statistics(chan statisticsChannel, int cores) {
    int connections = 0;
    int active = 0;
    int failed = 0;
    
    while (1) {
        int const op = chr(statisticsChannel, int);

        if (op == CONN_ESTABLISHED)
            ++connections, ++active;
        else
            --active;
        if (op == CONN_FAILED)
            ++failed;

        clearScreen();
        printf("Process ID: %d -- Active connections: %d\n", (int)getpid(), active);
        printf("History:\n");
        printf(" - Total number of connections: %d\n", connections);
        printf(" - Failed connections: %d\n\n", failed);
    }
}

int main(int argc, char *argv[]) {
    int port = 5555;
    int cores = 1;

    if (argc > 1)
        port = atoi(argv[1]);
    if (argc > 2)
        cores = atoi(argv[2]);

    ipaddr address = iplocal(NULL, port, 0);
    tcpsock listeningSocket = tcplisten(address, 10);
    if (!listeningSocket) {
        perror("Error opening socket");
        return 1;
    }

    int i;
    for (i = 0; i < cores - 1; ++i) {
        pid_t pid = mfork();
        if (pid < 0) {
           perror("Can't create new process");
           return 1;
        }
        if (pid == 0) {
            break;
        }
    }

    chan statisticsChannel = chmake(int, 0);
    go(statistics(statisticsChannel, cores));

    while (1) {
        tcpsock connection = tcpaccept(listeningSocket, NO_DEADLINE);
        if (connection) {
            go(dialogue(connection, statisticsChannel));
        }
    }
}
