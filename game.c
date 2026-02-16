#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>
#include <time.h>

#define BOARD_SIZE 16
#define MAX_LENGTH 256
#define SLEEP_USEC 200000

typedef struct Point {
    int x, y;
} Point;

typedef struct Snake {
    int velX, velY;
    int length;
    Point* body[MAX_LENGTH];
} Snake;

Snake snake;
Point apple;
pthread_mutex_t snakeLock = PTHREAD_MUTEX_INITIALIZER;
struct termios oldt, newt;
int gameOver = 0;

// ---------- Utils ----------
int comparePoints(Point* a, Point* b) {
    return a->x == b->x && a->y == b->y;
}

void placeApple() {
    int collision;
    do {
        collision = 0;
        apple.x = rand() % BOARD_SIZE + 1;
        apple.y = rand() % BOARD_SIZE + 1;

        for(int i=0; i < snake.length; i++) {
            if(comparePoints(&apple, snake.body[i])) {
                collision = 1;
                break;
            }
        }
    } while(collision);
}

// ---------- Game Logic ----------
void init() {
    srand(time(NULL));

    Point* head = malloc(sizeof(Point));
    head->x = 4;
    head->y = 8;

    snake.body[0] = head;
    snake.length = 1;
    snake.velX = 1;
    snake.velY = 0;

    placeApple();
}

void destroySnake() {
    for(int i=0; i < snake.length; i++)
        free(snake.body[i]);
}

void draw() {
    printf("\033[H\033[J"); // clear screen

    for(int y = 1; y <= BOARD_SIZE; y++) {
        for(int x = 1; x <= BOARD_SIZE; x++) {
            Point pt = {x, y};
            int printed = 0;

            pthread_mutex_lock(&snakeLock);
            for(int i=0; i < snake.length; i++) {
                if(comparePoints(&pt, snake.body[i])) {
                    putchar(i==0 ? 'O' : 'o');
                    printed = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&snakeLock);

            if(!printed && comparePoints(&pt, &apple)) {
                putchar('@');
                printed = 1;
            }

            if(!printed)
                putchar('.');
        }
        putchar('\n');
    }
    printf("WASD to move, q to quit!\n");
}

// ---------- Input ----------
void* checkKeys(void* arg) {
    while(!gameOver) {
        char ch = getchar();
        pthread_mutex_lock(&snakeLock);
        switch(ch) {
            case 'q': gameOver = 1; break;
            case 'w': if(snake.velY != 1) { snake.velX=0; snake.velY=-1; } break;
            case 's': if(snake.velY != -1){ snake.velX=0; snake.velY=1; } break;
            case 'a': if(snake.velX != 1){ snake.velX=-1; snake.velY=0; } break;
            case 'd': if(snake.velX != -1){ snake.velX=1; snake.velY=0; } break;
        }
        pthread_mutex_unlock(&snakeLock);
        usleep(50000); // tiny sleep to reduce CPU spin
    }
    return NULL;
}

// ---------- Main Loop ----------
void updateSnake() {
    pthread_mutex_lock(&snakeLock);

    // Move body
    for(int i = snake.length - 1; i > 0; i--) {
        snake.body[i]->x = snake.body[i-1]->x;
        snake.body[i]->y = snake.body[i-1]->y;
    }

    // Move head
    snake.body[0]->x += snake.velX;
    snake.body[0]->y += snake.velY;

    // Wall collision
    if(snake.body[0]->x < 1 || snake.body[0]->x > BOARD_SIZE ||
       snake.body[0]->y < 1 || snake.body[0]->y > BOARD_SIZE)
        gameOver = 1;

    // Self collision
    for(int i=1; i<snake.length; i++)
        if(comparePoints(snake.body[0], snake.body[i]))
            gameOver = 1;

    // Apple collision
    if(comparePoints(snake.body[0], &apple) && snake.length < MAX_LENGTH) {
        Point* tail = malloc(sizeof(Point));
        tail->x = snake.body[snake.length-1]->x;
        tail->y = snake.body[snake.length-1]->y;
        snake.body[snake.length++] = tail;
        placeApple();
    }

    pthread_mutex_unlock(&snakeLock);
}

int main() {
    // Terminal raw mode
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    init();

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, checkKeys, NULL);

    while(!gameOver) {
        draw();
        updateSnake();
        usleep(SLEEP_USEC);
    }

    draw();
    printf("GAME OVER!\n");

    pthread_cancel(thread_id);
    pthread_join(thread_id, NULL);

    destroySnake();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return 0;
}
