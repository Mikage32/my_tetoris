#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "sys/time.h"
#include "errno.h"
#include "termios.h"
#include "unistd.h"
#include "sys/types.h"
#include "signal.h"
#include "memory.h"

extern int errno;
struct termios otty,ntty;

#define SIZE_V 20
#define SIZE_H 10
#define BACKGROUND 47
#define COLOR_Z 41
#define COLOR_S 42
#define COLOR_L 40
#define COLOR_J 44
#define COLOR_T 45
#define COLOR_I 46
#define COLOR_O 43
#define ID_Z 0
#define ID_S 1
#define ID_L 2
#define ID_J 3
#define ID_T 4
#define ID_I 5
#define ID_O 6
#define ROTATE_L 1
#define ROTATE_R -1
#define MOVE_L 1
#define MOVE_R 2
#define MOVE_ROTATE_L 4
#define MOVE_ROTATE_R 8
#define MOVE_SOFT_DROP 16
#define MOVE_HARD_DROP 32
#define MOVE_HOLD 64
#define ESC 0x1b
#define QUIT 4096
#define FPS 60

struct mino{
    int mino_id;
    int x;
    int y;
    int rotate;
};

int next_mino(int mino, int l_0, int k, int i);
int initialize(int cell_color[SIZE_V][SIZE_H], int pre_cell_color[SIZE_V][SIZE_H], struct mino* current_mino, int* l_0, int* k);
int process_input(void);
int rotate_mino(struct mino* current_mino, int lr);
int move_mino(struct mino* current_mino);
int check_line(int cell_color[SIZE_V][SIZE_H]);
int repaint(int cell_color[SIZE_V][SIZE_H], int pre_cell_color[SIZE_V][SIZE_H], int score, int hold, int next);
int update(int cell_color[SIZE_V][SIZE_H], int pre_cell_color[SIZE_V][SIZE_H], int score, int hold, int next, int* isRunning);
int tetoris(void);

static void onsignal(int sig) {
    signal(sig, SIG_IGN);
    switch(sig){
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
    case SIGHUP:
      exit(1);
      break;
    }
}
int tinit(void) {
    if (tcgetattr(1, &otty) < 0)
      return -1;
    ntty = otty;
    ntty.c_iflag &= ~(INLCR|ICRNL|IXON|IXOFF|ISTRIP);
    ntty.c_oflag &= ~OPOST;
    ntty.c_lflag &= ~(ICANON|ECHO);
    ntty.c_cc[VMIN] = 1;
    ntty.c_cc[VTIME] = 0;
    tcsetattr(1, TCSADRAIN, &ntty);
    signal(SIGINT, onsignal);
    signal(SIGQUIT, onsignal);
    signal(SIGTERM, onsignal);
    signal(SIGHUP, onsignal);
}
void reset(void) {
    tcsetattr(1, TCSADRAIN, &otty);
    write(1, "\n", 1);
}
int kbhit(void) {
    int ret;
    fd_set rfd;
    struct timeval timeout = {0,0};
    FD_ZERO(&rfd);
    FD_SET(0, &rfd);
    ret = select(1, &rfd, NULL, NULL, &timeout);
    if (ret == 1)
      return 1;
    else
      return 0;
}
int getch(void) {
    unsigned char c;
    int n;
    while ((n = read(0, &c, 1)) < 0 && errno == EINTR) ;
    if (n == 0)
      return -1;
    else
      return (int)c;
}

int main(void){
    tinit();
    tetoris();

    reset();
    return 0;
}

int next_mino(int mino, int l_0, int k, int i){
    return (mino + (l_0+k*(i/7))%6 + 1)%7;
}

int initialize(int cell_color[SIZE_V][SIZE_H], int pre_cell_color[SIZE_V][SIZE_H], struct mino* current_mino, int* l_0, int* k){
    system("clear");
    
    srand(time(NULL));
    current_mino->mino_id = rand()%7;
    *l_0 = rand()%7;
    *k = rand()%6;
    if(*k == 0) *k += 1;
    
    for(int y = 0; y < SIZE_V; ++y){
        for(int x = 0; x < SIZE_H; ++x){
            cell_color[y][x] = BACKGROUND;
            pre_cell_color[y][x] = 0;
        }
    }

    printf("\n\n");
    for(int i = 0;i < SIZE_V; ++i) printf("\n");
    printf("\n");

    repaint(cell_color, pre_cell_color, 0, 7, next_mino(current_mino->mino_id, *l_0, *k, 0));

    for(int y = 0; y < SIZE_V; ++y){
        for(int x = 0; x < SIZE_H; ++x){
            pre_cell_color[y][x] = BACKGROUND;
        }
    }

    return 0;
}

int rotate_mino(struct mino* current_mino, int lr){
    current_mino->rotate += 4+lr;
    current_mino->rotate %= 4;
    
    return 0;
}

int check_line(int cell_color[SIZE_V][SIZE_H]){
    int counter = 0;
    for(int y = 0; y < SIZE_V; ++y){
        int flag = 1;
        for(int x = 0; x < SIZE_H; ++x){
            if(cell_color[y][x] == BACKGROUND) {
                flag = 0;
                break;
            }
        }
        if(flag){
            ++counter;
            for(int i = y; i > 0; ++i){
                memcpy(cell_color[i], cell_color[i-1], sizeof(int)*SIZE_H);
            }
            if(y != 0) {
                for(int i = 0; i < SIZE_H; ++i) cell_color[0][i] = BACKGROUND;
            }
        }
    }

    return counter;
}

int repaint(int cell_color[SIZE_V][SIZE_H], int pre_cell_color[SIZE_V][SIZE_H], int score, int hold, int next){
    printf("\e[%dA", 1+SIZE_V+2);
    
    printf("\e[2K");
    printf("\rHOLD: %d\n", hold);
    printf("\e[2K");
    printf("\rNEXT: %d\n", next);
    
    for(int y = 0; y < SIZE_V; ++y){
        printf("\r");
        for(int x = 0; x < SIZE_H; ++x){
            if(pre_cell_color[y][x] != cell_color[y][x]){
                printf("\x1b[%dm  \x1b[m", cell_color[y][x]);
            }else{
                printf("\e[1C");
            }
        }
        printf("\n");
    }
    
    printf("\x1b[0m");
    printf("\rSCORE: %d\n\r", score);

    return 0;
}

int process_input(void){
    int bitflag = 0;

    if(kbhit()){
        char c = getch(), t;
    
        switch (c) {
        case 0x1b:
            if(kbhit()){
                getch();
                t = getch();
                switch (t) {
                case 'D':
                    bitflag |= MOVE_L;
                    break;
                case 'C':
                    bitflag |= MOVE_R;
                    break;
                case 'A':
                    bitflag |= MOVE_ROTATE_L;
                    break;
                case 'B':
                    bitflag |= MOVE_SOFT_DROP;
                    break;
                default:
                    break;
                }
            }else{
                bitflag |= QUIT;
            }
            break;
        case 'z':
            bitflag |= MOVE_ROTATE_L;
            break;
        case 'x':
            bitflag |= MOVE_ROTATE_R;
            break;
        case 'c':
            bitflag |= MOVE_HOLD;
            break;
        case ' ':
            bitflag |= MOVE_HARD_DROP;
            break;
        default:
            break;
        }
    }
    
    return bitflag;
}

int update(int cell_color[SIZE_V][SIZE_H], int pre_cell_color[SIZE_V][SIZE_H], int score, int hold, int next, int* isRunning){
    for(int i = 0; i < SIZE_V; ++i) memcpy(pre_cell_color[i], cell_color[i], sizeof(int)*SIZE_H);
    
    int bitflag = process_input();
    if(bitflag&QUIT){
        *isRunning = 0;
        return 0;
    }
    if(bitflag&MOVE_L) printf("\e[2KL");
    if(bitflag&MOVE_R) printf("\e[2KR");
    if(bitflag&MOVE_ROTATE_L) printf("\e[2KRL");
    if(bitflag&MOVE_ROTATE_R) printf("\e[2KRR");
    if(bitflag&MOVE_HOLD) printf("\e[2KHOLD");
    if(bitflag&MOVE_SOFT_DROP) printf("\e[2KSOFT DROP");
    if(bitflag&MOVE_HARD_DROP) printf("\e[2KHARD DROP");

    check_line(cell_color);

    repaint(cell_color, pre_cell_color, score, hold, next);

    return 0;
}

int tetoris(void){
    int l_0, k, i = 0, hold = 7, score = 0, isRunning = 1;
    struct mino current_mino;
    struct timeval pre_time, current_time;
    int cell_color[SIZE_V][SIZE_H];
    int pre_cell_color[SIZE_V][SIZE_H];
    
    initialize(cell_color, pre_cell_color, &current_mino, &l_0, &k);

    gettimeofday(&pre_time, NULL);
    while(isRunning){
        gettimeofday(&current_time, NULL);
        if(pre_time.tv_sec*1000+pre_time.tv_usec/1000 - current_time.tv_sec*1000+current_time.tv_usec/1000 < 1000/FPS){
            continue;
        }else{
            pre_time = current_time;
        }

        update(cell_color, pre_cell_color, score, hold, next_mino(current_mino.mino_id, l_0, k, i), &isRunning);
    }
    
    system("clear");
    printf("\rGAME OVER\n\rSCORE: %d\n", score);
    
    return 0;
}