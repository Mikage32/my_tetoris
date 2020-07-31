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

struct seed{
    int l_0;
    int k;
    int i;
};

struct gameInfo {
    struct seed mino_seed;
    struct mino current_mino;
    int score;
    int hold;
    int isRunning;
    long long frame;
};

int next_mino(struct gameInfo* info);
int initialize(int cell_color[SIZE_V][SIZE_H], struct gameInfo* info);
int process_input(void);
int rotate_mino(int cell_color[SIZE_V][SIZE_H],struct mino* current_mino, int lr);
int move_mino(int cell_color[SIZE_V][SIZE_H], struct mino* current_mino);
int check_line(int cell_color[SIZE_V][SIZE_H]);
int repaint(int cell_color[SIZE_V][SIZE_H], struct gameInfo* info);
int update(int cell_color[SIZE_V][SIZE_H], struct gameInfo* info);
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

int next_mino(struct gameInfo* info){
    return (info->current_mino.mino_id + (info->mino_seed.l_0+info->mino_seed.k*(info->mino_seed.i/7))%6 + 1)%7;
}

int initialize(int cell_color[SIZE_V][SIZE_H], struct gameInfo* info){
    system("clear");
    srand(time(NULL));

    info->current_mino.mino_id = rand()%7;
    info->current_mino.rotate = 0;
    info->current_mino.x = 4;
    info->current_mino.y = 1;
    
    info->mino_seed.l_0 = rand()%7;
    info->mino_seed.k = rand()%6;
    if(info->mino_seed.k == 0) info->mino_seed.k += 1;
    info->mino_seed.i = 0;

    info->hold = 7;
    info->isRunning = 1;
    info->score = 0;
    info->frame = 0;
    
    for(int y = 0; y < SIZE_V; ++y){
        for(int x = 0; x < SIZE_H; ++x){
            cell_color[y][x] = BACKGROUND;
        }
    }

    printf("\n\n");
    for(int i = 0;i < SIZE_V; ++i) printf("\n");
    printf("\n");

    repaint(cell_color, info);

    return 0;
}

/**
* 操作中のミノを指定方向に回転させる. 
* ただし, 回転できない状況である場合は回転を行わず, -1を返す. 
*/
int rotate_mino(int cell_color[SIZE_V][SIZE_H], struct mino* current_mino, int lr){
    
    
    current_mino->rotate += 4+lr;
    current_mino->rotate %= 4;
    
    return 0;
}

int move_mino(int cell_color[SIZE_V][SIZE_H], struct mino* current_mino, int bitflag){

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

int repaint(int cell_color[SIZE_V][SIZE_H], struct gameInfo* info){
    printf("\e[%dA", 1+SIZE_V+2);
    
    printf("\e[2K");
    printf("\rHOLD: %d\n", info->hold);
    printf("\e[2K");
    printf("\rNEXT: %d\n", next_mino(info));
    
    for(int y = 0; y < SIZE_V; ++y){
        printf("\r");
        for(int x = 0; x < SIZE_H; ++x){
            printf("\x1b[%dm  \x1b[m", cell_color[y][x]);
        }
        printf("\n");
    }
    
    printf("\x1b[0m");
    printf("\rSCORE: %d\n\r", info->score);

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

int update(int cell_color[SIZE_V][SIZE_H], struct gameInfo* info){
    info->frame += 1;
    int bitflag = process_input();
    if(bitflag&QUIT){
        info->isRunning = 0;
        return 0;
    }
    
    int isMoved = move_mino(cell_color, &(info->current_mino), bitflag);
    //動いていなければ, そのときの処理をおこなう(後で書く)

    check_line(cell_color);
    repaint(cell_color, info);

    return 0;
}

int tetoris(void){
    struct gameInfo info;

    struct timeval pre_time, current_time;
    int cell_color[SIZE_V][SIZE_H];
    
    initialize(cell_color, &info);

    gettimeofday(&pre_time, NULL);
    while(info.isRunning){
        gettimeofday(&current_time, NULL);
        if(pre_time.tv_sec*1000+pre_time.tv_usec/1000 - current_time.tv_sec*1000+current_time.tv_usec/1000 < 1000/FPS){
            continue;
        }else{
            pre_time = current_time;
        }

        update(cell_color, &info);
    }
    
    system("clear");
    printf("\rGAME OVER\n\rSCORE: %d\n", info.score);
    
    return 0;
}