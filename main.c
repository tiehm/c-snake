#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <time.h>
#include <errno.h>

#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>

pthread_t tid[2];
int counter;
pthread_mutex_t instruction_lock;

void sleep_ms(int milliseconds){ // cross-platform sleep function
#ifdef WIN32
    Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    if (milliseconds >= 1000)
        sleep(milliseconds / 1000);
    usleep((milliseconds % 1000) * 1000);
#endif
}

typedef struct {
    int x;
    int y;
} point;

WINDOW * mainwin;

int game_tick_ms = 100;

int instructionN = 0;
int * instructions;

int running = 1;

int direction = KEY_RIGHT;

int coordinateN = 1;
int coordinateCapacity = 16;
point * coordinates;

int WIDTH = 30;
int HEIGHT = 25;
// LULW memory optimization, what's that
char score[100];

point apple;

void *handleKeyboardInputs() {
    int ch;
    while ((ch = getch())) {
        if (ch == 'q') {
            delwin(mainwin);
            endwin();
            refresh();
            exit(0);
        }
        if (ch == -1 || instructionN > 31) {
            sleep_ms(5);
            continue;
        }
        if ((ch == KEY_UP && !(direction == KEY_RIGHT || direction == KEY_LEFT)) ||
                ch == KEY_DOWN && !(direction == KEY_RIGHT || direction == KEY_LEFT) ||
                ch == KEY_LEFT && !(direction == KEY_DOWN || direction == KEY_UP) ||
                ch == KEY_RIGHT && !(direction == KEY_DOWN || direction == KEY_UP)) {
            sleep_ms(5);
            continue;
        }
        pthread_mutex_lock(&instruction_lock);
        instructionN++;
        instructions[instructionN-1] = ch;
        pthread_mutex_unlock(&instruction_lock);
    }
    return NULL;
}

void shift_instructions() {
    for (int i = 0; i < instructionN; ++i) {
        instructions[i] = instructions[i+1];
    }
}

point reset_out_of_bounds(point p) {
    if (p.x < 0) {
        p.x = WIDTH;
    }
    if (p.x > WIDTH) {
        p.x = 0;
    }
    if (p.y < 0) {
        p.y = HEIGHT;
    }
    if (p.y > HEIGHT) {
        p.y = 0;
    }
    return p;
}

point point_move(point p) {
    if (direction == KEY_UP) {
        p.y--;
    }
    if (direction == KEY_DOWN) {
        p.y++;
    }
    if (direction == KEY_LEFT) {
        p.x--;
    }
    if (direction == KEY_RIGHT) {
        p.x++;
    }

    return reset_out_of_bounds(p);
}

void *gameEngine() {
    while (running) {
        pthread_mutex_lock(&instruction_lock);
        if (instructions[0] != 0) {
            direction = instructions[0];
            shift_instructions();
            instructionN--;
        }
        pthread_mutex_unlock(&instruction_lock);
        coordinates[coordinateN] = point_move(coordinates[coordinateN-1]);
        clear();
        if (coordinates[coordinateN].x == apple.x && coordinates[coordinateN].y == apple.y) {
            coordinateN++;
            if (coordinateN > coordinateCapacity) {
                coordinateCapacity *= 2;
                coordinates = realloc(coordinates, coordinateCapacity * sizeof(point));
            }
            coordinates[coordinateN] = point_move(coordinates[coordinateN-1]);
            apple.x = rand() % WIDTH;
            apple.y = rand() % HEIGHT;
        }
        for (int i = 0; i < coordinateN; ++i) {
            if (i > 1 && coordinates[i].x == coordinates[coordinateN].x && coordinates[i].y == coordinates[coordinateN].y) {
                pthread_cancel(tid[0]);
                running = 0;
                break;
            }
            coordinates[i] = coordinates[i+1];
            mvwaddstr(mainwin, coordinates[i].y, coordinates[i].x, "X");
        }
        mvwaddstr(mainwin, apple.y, apple.x, "@");

        sprintf(score, "Score: %d", coordinateN);
        mvwaddstr(mainwin, HEIGHT+4, WIDTH/2, score);
        sleep_ms(game_tick_ms);
    }
    return NULL;
}

int main(void) {

    if (pthread_mutex_init(&instruction_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    apple.x = rand() % WIDTH;
    apple.y = rand() % HEIGHT;

    instructions = (int *) malloc(sizeof (int) * 32);

    time_t t;
    srand((unsigned) time(&t));

    coordinates = (point *)malloc(sizeof(point)*coordinateCapacity);
    if (coordinates == NULL) {
        fprintf(stderr, "Error initialising coordinates.\n");
        exit(EXIT_FAILURE);
    }
    point p;
    p.x = 1;
    p.y = 1;
    coordinates[0] = p;
    coordinateN = 1;

    if ( (mainwin = initscr()) == NULL ) {
        fprintf(stderr, "Error initialising ncurses.\n");
        exit(EXIT_FAILURE);
    }

    noecho();
    nodelay(stdscr, true);
    keypad(mainwin, TRUE);
    refresh();

    int err;

    err = pthread_create(&(tid[0]), NULL, &handleKeyboardInputs, NULL);
    if (err != 0)
        printf("\ncan't create thread :[%s]", strerror(err));

    err = pthread_create(&(tid[1]), NULL, &gameEngine, NULL);
    if (err != 0)
        printf("\ncan't create thread :[%s]", strerror(err));

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_mutex_destroy(&instruction_lock);

    delwin(mainwin);
    endwin();
    refresh();
    printf("Game over, you suck. Score: %d\n", coordinateN);
    return EXIT_SUCCESS;
}
