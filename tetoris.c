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

int mino_graph[8][4][4] = {
    {
        {COLOR_Z, COLOR_Z, BACKGROUND, BACKGROUND}, 
        {BACKGROUND, COLOR_Z, COLOR_Z, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND}
    },
    {
        {BACKGROUND, COLOR_S, COLOR_S, BACKGROUND},
        {COLOR_S, COLOR_S, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND}
    },
    {
        {BACKGROUND, BACKGROUND, COLOR_L, BACKGROUND},
        {COLOR_L, COLOR_L, COLOR_L, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND}
    },
    {
        {COLOR_J, BACKGROUND, BACKGROUND, BACKGROUND},
        {COLOR_J, COLOR_J, COLOR_J, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND}
    },
    {
        {BACKGROUND, COLOR_T, BACKGROUND, BACKGROUND},
        {COLOR_T, COLOR_T, COLOR_T, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND}
    },
    {
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {COLOR_I, COLOR_I, COLOR_I, COLOR_I},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND}
    },
    {
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, COLOR_O, COLOR_O, BACKGROUND},
        {BACKGROUND, COLOR_O, COLOR_O, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND}
    },
    {
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND},
        {BACKGROUND, BACKGROUND, BACKGROUND, BACKGROUND}
    }
};

int cell_color[SIZE_V][SIZE_H];

struct mino{
    int mino_id;
    int x;
    int y;
    int rotate;
    int stoping_since;
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
int initialize(struct gameInfo* info);
int process_input(void);
int rotate_graph(int graph[4][4]);
int graph_mino(int graph[4][4], struct mino* current_mino);
int put_mino(struct mino* current_mino);
int move_mino(struct mino* current_mino, int bitflag);
int check_line(void);
int repaint(struct gameInfo* info);
int update(struct gameInfo* info);
int tetoris(void);

/*  
    入力をリアルタイムに行うために非カノニカルモードに設定する処理群及び, 
    文字入力読み込みの為の関数
    サンプルプログラムを参考に一部を改変しつつ作成した. 
*/
/*******************************************************************************/

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

/*******************************************************************************/

int main(void){
    tinit();
    tetoris();

    reset();
    return 0;
}

/**
 * game infoの内容から, NEXT MINOを計算して求める. 
 * ミノは全部で7種類であり, それぞれを0~6の番号をつけて呼ぶとして, 第i番目のミノmは次の式で表される. 
 * m_i = (m_{i-1}+l_{i/7}+1)%7
 * l_i = (l_0 + k*i)%6 = (l_{i-1}+k)%6
 * m_0, l_0, kはゲーム開始時にランダムに決められる. ただし, kはmod6において0でないこととする. 
*/
int next_mino(struct gameInfo* info){
    return (info->current_mino.mino_id + (info->mino_seed.l_0+info->mino_seed.k*(info->mino_seed.i/7))%6 + 1)%7;
}

/**
 * 盤面を含むゲーム情報全般を初期化する. 
*/
int initialize(struct gameInfo* info){
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
    put_mino(&(info->current_mino));

    printf("\n\n");
    for(int i = 0;i < SIZE_V; ++i) printf("\n");
    printf("\n");

    repaint(info);

    return 0;
}

/**
 * 反時計回りにグラフを90度回転させる. 
*/
int rotate_graph(int graph[4][4]){
    int rotated_graph[4][4];

    for(int i = 0; i < 3; ++i){
        for(int j = 0; j < 4; ++j){
            rotated_graph[j][2-i] = graph[i][j];
        }
    }

    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 3; ++j){
            graph[i][j] = rotated_graph[i][j];
        }
        graph[i][3] = BACKGROUND;
    }

    return 0;
}

/**
* 指定ミノを指定方向に回転させたときのミノの図を計算する. 
*/
int graph_mino(int graph[4][4], struct mino* current_mino){
    graph = mino_graph[current_mino->mino_id];

    if(current_mino->mino_id != ID_O){
        for(int i = 0; i < current_mino->rotate; ++i) rotate_graph(graph);
    }
    
    return 0;
}

/**
 * ミノを盤面上に置く. 
 * ミノを指定位置に設置することができない場合は-1を返す. 
*/
int put_mino(struct mino* current_mino){
    int graph[4][4];
    graph_mino(graph, current_mino);

    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            if(graph[i][j] == BACKGROUND) continue;
            else if(current_mino->x - 1 + j < 0 || current_mino->x - 1 + j >= SIZE_H
                || current_mino->y - 1 + i < 0 || current_mino->y - 1 + i >= SIZE_V){
                return -1;
            }else if(cell_color[current_mino->y-1+i][current_mino->x-1+j] != BACKGROUND){
                return -1;
            }
        }
    }

    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            if(current_mino->x - 1 + j < 0 || current_mino->x - 1 + j >= SIZE_H
                || current_mino->y - 1 + i < 0 || current_mino->y - 1 + i >= SIZE_V){
                
                cell_color[current_mino->y-1+i][current_mino->x-1+j] = COLOR_I;//graph[i][j];
            }
        }
    }

    return 0;
}

/**
 * 操作中のミノをbitflagの内容に従って動かして, 0を返す. 
 * ただし, ミノが動かせない状況である場合はミノを動かさず, -1を返す. 
*/
int move_mino(struct mino* current_mino, int bitflag){
    struct mino mino_cpy = *current_mino;
    
    if(bitflag&MOVE_ROTATE_L) current_mino->rotate = (current_mino->rotate + 4 + 1)%4;
    if(bitflag&MOVE_ROTATE_R) current_mino->rotate = (current_mino->rotate + 4 - 1)%4;
    if(bitflag&MOVE_L) current_mino->x -= 1;
    if(bitflag&MOVE_R) current_mino->x += 1;
    if(bitflag&MOVE_SOFT_DROP) current_mino->y += 1;

    //判定関数を利用
    
    return 0;
}

int check_line(void){
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

/**
 * cell_color及びgmae infoの情報を元に, ゲーム画面を表示する. 
 * ゲーム画面は, 第1行目にHOLD MINO, 第2行目にNEXT MINO
 * 第3行目からSIZE_H(=20)行にわたり盤面を行事する. 
 * 第23行目にはSCOREを表示する. 
*/
int repaint(struct gameInfo* info){
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

/**
 * キーボード入力を処理してフラグ情報に変換する. 
 * 何も操作が無いときフラグは0である. 
*/
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

/**
 * ゲームを1フレーム更新する. 
 * 1. キー入力
 * 2. キー入力に応じた処理
 * 3. 盤面の変更確認と処理
 * 4. 描画
 * の順で処理を行う.
*/
int update(struct gameInfo* info){
    info->frame += 1;
    int bitflag = process_input();
    if(bitflag&QUIT){
        info->isRunning = 0;
        return 0;
    }
    
    int isMoved = move_mino(&(info->current_mino), bitflag);
    //動いていなければ, そのときの処理をおこなう(後で書く)

    check_line();
    repaint(info);

    return 0;
}

int tetoris(void){
    struct gameInfo info;

    struct timeval pre_time, current_time;
    
    initialize(&info);

    gettimeofday(&pre_time, NULL);
    while(info.isRunning){
        gettimeofday(&current_time, NULL);
        if(pre_time.tv_sec*1000+pre_time.tv_usec/1000 - current_time.tv_sec*1000+current_time.tv_usec/1000 < 1000/FPS){
            continue;
        }else{
            pre_time = current_time;
        }

        update(&info);
    }
    
    system("clear");
    printf("\rGAME OVER\n\rSCORE: %d\n", info.score);
    
    return 0;
}