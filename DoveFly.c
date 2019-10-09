#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <malloc.h>
#include <memory.h>
#include <time.h>

/**********************************************************************
*                               macros                               *
**********************************************************************/
#define MAX(x,y) (((x)>(y))?(x):(y))
#define MIN(x,y) (((x)<(y))?(x):(y))

/**********************************************************************
*                         Objects Properties                         *
**********************************************************************/
#define FPS 60       // frame per second
#define GRAVITY 0.01 // gravity in the game
#define MIN_V -0.3   // the max velocity for bird to fly up
#define MAX_V 0.3    // the max velocity for birf to drop down
#define BAR_VX -0.2   // the velocity of barriers
#define BAR_VY 0.1

/**********************************************************************
*                            Objects Size                            *
**********************************************************************/
#define VALLEY_W 80 // valley width
#define VALLEY_H 30 // valley height
#define PANEL_W 80  // panel width
#define PANEL_H 10  // panel height
#define BAR_W 10    // barrier width
#define BAR_H 40    // barrier height
#define OVER_W 53   // gameover window width
#define OVER_H 7    // gameover window height
#define START_W 65
#define START_H 6
#define BAR_SEPH_MIN 10
#define BAR_SEPH_MAX 30
#define BAR_SEPV_MIN 10
#define BAR_SEPV_MAX 20

/**********************************************************************
*                               Objects                              *
**********************************************************************/
typedef struct {
    unsigned int score;
    unsigned int over;
    unsigned int fn;
    unsigned int fps;
    float dist;
} Score;

typedef struct {
    float x;
    float y;
    int w;
    int h;
} Object;

typedef struct {
    float x;
    float y;
    int w;
    int h;
    char *s;
} String;

typedef struct _Role {
    float x;
    float y;
    int w;
    int h;
    char *skin;
    int (*load_skin)(struct _Role *this, char *file);
} Role;

typedef struct _Anime {
    float x;
    float y;
    int w;
    int h;
    char *skin;
    unsigned int fn; // 总共的帧数
    unsigned int cf; // 当前的帧
    unsigned int it; // 多少游戏帧刷新一次
    unsigned int ci; // 当前游戏帧
    char **frames;
    int (*load_frames)(struct _Anime *this, char *file);
} Anime;

typedef struct _Window {
    Role *p;
    char *pixel;
    void (*draw_self)(struct _Window *this);
    void (*draw_role)(struct _Window *this, Role *role);
    void (*draw_string)(struct _Window *this, int sx, int sy, char *s);
    void (*sync_screen)(struct _Window *this);
} Window;

typedef struct {
    Anime *p;
    float v;
} Bird;

typedef struct _Barrier {
    Role *p;
    int sep;
    float vx;
    float vy;
    struct _Barrier *next;
} Barrier;

typedef struct _BarrierManager {
    Barrier *head;
    Barrier **tail;
    Barrier *pool;
    void (*check_barrier)(struct _BarrierManager *this, Window *win, Score *score);
    void (*add_barrier)(struct _BarrierManager *this, Window *win);
    void (*del_barrier)(struct _BarrierManager *this);
} BarrierManager;

typedef struct {
    Window *valley;
    Window *panel;
    Bird *bird;
    BarrierManager *barMgr;
    Score *score;
} Args;

/**********************************************************************
*                        function declaration                        *
**********************************************************************/
// declarations for class methods
int load_skin(Role *this, char *file);
int load_frames(Anime *this, char *file);
void draw_self(Window *this);
void draw_role(Window *this, Role *role);
void sync_screen(Window *this);
void check_barrier(BarrierManager *this, Window *win, Score *score);
void add_barrier(BarrierManager *this, Window *win);
void del_barrier(BarrierManager *this);

// declarations for create and destroy
void setup_object(Object *obj, float x, float y, int w, int h);
Anime *create_anime(float x, float y, int w, int h, int fn, int it, char *file);
void destroy_anime(Anime **anime);
Role *create_role(float x, float y, int w, int h, char *file);
void destroy_role(Role **role);
Window *create_window(float x, float y, int w, int h, char *file);
void destroy_window(Window **win);
Bird *create_bird(float x, float y, int w, int h, char *file);
void reset_bird(Bird *bird, float x, float y);
void destroy_bird(Bird **bird);
Barrier *create_barrier(float x, float y, int w, int h, float vx, float vy, char *file);
void destroy_barrier(Barrier **bar);
BarrierManager *create_barrier_manager();
void reset_barrier_manager(BarrierManager *barMgr);
void destroy_barrier_manager(BarrierManager **barMgr);
Score *create_score();
void reset_score(Score *score);
void destroy_score(Score **score);

// generate a random int in range of [start, end)
int randint(int start, int end);
void init_game();
void new_game(Window *valley, Window *panel, Role *start, BarrierManager *barMgr, Bird *bird, Score *score, Args *args);
void update_bird(Bird *bird);
void update_barriers(Window *win, BarrierManager *barMgr, Score *score);
int collision_detect(Window *win, Bird *bird, BarrierManager *barMgr);
void loop(Bird *bird, Score *score);
void *play(void *_args);
void *count(void *_args);

/**********************************************************************
*                      Objects Implementations                       *
**********************************************************************/
// role
int load_skin(Role *this, char *file)
{
    // allocate some memory for buffer, the size of buffer must be more
    // than (w + 2), one byte for '\n', one byte for '\0'
    char * buff = (char *) malloc((this->w + 2) * sizeof(char));
    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        perror(file);
        return 0;
    }

    for (int y = 0; y < this->h; y++) {
        fgets(buff, this->w + 2, fp);
        for (int x = 0; x < this->w; x++) {
            this->skin[y * this->w + x] = buff[x];
        }
        if (feof(fp)) break;
    }

    free(buff);
    fclose(fp);
    return 1;
}

int load_frames(Anime *this, char *file)
{
    char *buff = (char *) malloc((this->w + 2) * sizeof(char));
    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        perror(file);
        return 0;
    }

    for (int f = 0; f < this->fn; f++) {
        for (int y = 0; y < this->h; y++) {
            fgets(buff, this->w + 2, fp);
            for (int x = 0; x < this->w; x++) {
                this->frames[f][y * this->w + x] = buff[x];
            }
            if (feof(fp)) break;
        }
    }

    free(buff);
    fclose(fp);
    return 1;
}

// Window
void draw_self(Window *this)
{
    for (int x = 0; x < this->p->w; x++) {
        for (int y = 0; y < this->p->h; y++) {
            int index = y * this->p->w + x;
            this->pixel[index] = this->p->skin[index];
        }
    }
}

void draw_role(Window *this, Role *role)
{
    // ignore the part that is out of window
    int xmin = MAX(0, 1 - role->x);
    int xmax = MIN(role->w, this->p->w - role->x - 1);
    int ymin = MAX(0, 1 - role->y);
    int ymax = MIN(role->h, this->p->h - role->y - 1);
    for (int x = xmin; x < xmax; x++) {
        for (int y = ymin; y < ymax; y++) {
            int index = (y + (int) role->y) * this->p->w + x + (int) role->x;
            this->pixel[index] = role->skin[(int)(y * role->w + x)];
        }
    }
}

void draw_string(struct _Window *this, int sx, int sy, char *s)
{
    if (sy <= 1 || sy >= this->p->h)
        return;

    int xmin = MAX(0, 1 - sx);
    for (int x = xmin; x < this->p->w - sx - 1; x++) {
        if (s[x] == 0)
            return;
        this->pixel[sy * this->p->w + sx + x] = s[x];
    }
}

void sync_screen(Window *this)
{
    for (int x = 0; x < this->p->w; x++) {
        for (int y = 0; y < this->p->h; y++) {
            move(this->p->y + y, this->p->x + x);
            addch(this->pixel[y * this->p->w + x]);
        }
    }
    refresh();
}

// BarrierManager
void check_barrier(BarrierManager *this, Window *win, Score *score)
{
    if (this->head == NULL)
        this->add_barrier(this, win);
    // the first barrier is out of window
    if (this->head->p->x + this->head->p->w < 0) {
        this->del_barrier(this);
        score->score++;
    }
    // the last barrier is move into screen, add a new barrier to the tail
    Barrier *tail = *this->tail;
    if (tail->p->x + tail->p->w < win->p->w)
        this->add_barrier(this, win);
}

void add_barrier(BarrierManager *this, Window *win)
{
    Barrier *barrier;
    if (this->pool) {
        barrier = this->pool;
        this->pool = this->pool->next;
    }
    else{
        barrier = create_barrier(BAR_SEPH_MIN, BAR_SEPV_MAX, BAR_W, BAR_H, BAR_VX, BAR_VY, "barrier.ascii");
    }

    barrier->p->x = win->p->w + randint(BAR_SEPH_MIN, BAR_SEPH_MAX);
    int sep = randint(BAR_SEPV_MIN, BAR_SEPV_MAX);
    barrier->sep = sep;
    barrier->p->y = randint(sep, win->p->h);
    barrier->vx = BAR_VX;
    barrier->vy = BAR_VY;
    barrier->next = NULL;

    if (*this->tail) {
        (*this->tail)->next = barrier;
        this->tail = &(*this->tail)->next;
    }
    else {
        *this->tail = barrier;
    }
}

void del_barrier(BarrierManager *this)
{
    if (this->head == NULL)
        return;

    Barrier *barrier = this->head;
    this->head = this->head->next;

    if (this->head == NULL)
        this->tail = &this->head;

    barrier->next = this->pool;
    this->pool = barrier;
}


/**********************************************************************
*                          global variables                          *
**********************************************************************/
// time to sleep
unsigned int interval = 1000000 / FPS;


/**********************************************************************
*                         create and destroy                         *
**********************************************************************/
void setup_object(Object *obj, float x, float y, int w, int h)
{
    obj->x = x;
    obj->y = y;
    obj->w = w;
    obj->h = h;
}

Anime *create_anime(float x, float y, int w, int h, int fn, int it, char *file)
{
    Anime *anime = (Anime *) malloc(sizeof(Anime));

    setup_object((Object *) anime, x, y, w, h);
    anime->fn = fn;
    anime->cf = 0;
    anime->it = it;
    anime->ci = 0;

    char **frames = (char **) malloc(sizeof(char *) * fn);
    for (int i = 0; i < anime->fn; i++) {
        frames[i] = (char *) malloc(sizeof(char) * w * h);
    }
    anime->frames = frames;
    anime->skin = anime->frames[anime->cf];
    anime->load_frames = load_frames;
    anime->load_frames(anime, file);

    return anime;
}

void destroy_anime(Anime **anime)
{
    for (int f = 0; f < (*anime)->fn; f++) {
        free((*anime)->frames[f]);
    }
    free((*anime)->frames);
    free(*anime);
    *anime = NULL;
}

Role *create_role(float x, float y, int w, int h, char *file)
{
    Role *role = (Role *) malloc(sizeof(Role));

    setup_object((Object *) role, x, y, w, h);
    role->load_skin = load_skin;

    char *skin = (char *) malloc(sizeof(char) * w * h);
    role->skin = skin;
    role->load_skin(role, file);

    return role;
}

void destroy_role(Role **role)
{
    free((*role)->skin);
    free(*role);
    *role = NULL;
}

Window *create_window(float x, float y, int w, int h, char *file)
{
    Window *win = (Window *) malloc(sizeof(Window));

    char *pixel = (char *) malloc(sizeof(char) * w * h);
    win->p = create_role(x, y, w, h, file);
    win->pixel = pixel;
    win->draw_self = draw_self;
    win->draw_role = draw_role;
    win->draw_string = draw_string;
    win->sync_screen = sync_screen;

    return win;
}

void destroy_window(Window **win)
{
    destroy_role(&(*win)->p);

    free((*win)->pixel);
    free(*win);
    *win = NULL;
}

Bird *create_bird(float x, float y, int w, int h, char *file)
{
    Bird *bird = (Bird *) malloc(sizeof(Bird));

    bird->p = create_anime(x, y, w, h, 2, 0, file);
    bird->v = 0;

    return bird;
}

void reset_bird(Bird *bird, float x, float y)
{
    bird->p->x = x;
    bird->p->y = y;
    bird->v = 0;
}

void destroy_bird(Bird **bird)
{
    destroy_anime(&(*bird)->p);

    free(*bird);
    *bird = NULL;
}

Barrier *create_barrier(float x, float y, int w, int h, float vx, float vy, char *file)
{
    Barrier *bar = (Barrier *) malloc(sizeof(Barrier));

    bar->p = create_role(x, y, w, h, file);
    bar->vx = vx;
    bar->vy = vy;
    bar->next = NULL;

    return bar;
}

void destroy_barrier(Barrier **bar)
{
    destroy_role(&(*bar)->p);

    free(*bar);
    *bar = NULL;
}

BarrierManager *create_barrier_manager()
{
    BarrierManager * barMgr = (BarrierManager *) malloc(sizeof(BarrierManager));

    barMgr->head = NULL;
    barMgr->tail = &barMgr->head;
    barMgr->pool = NULL;

    barMgr->check_barrier = check_barrier;
    barMgr->add_barrier = add_barrier;
    barMgr->del_barrier = del_barrier;

    return barMgr;
}

void reset_barrier_manager(BarrierManager *barMgr)
{
    if (*barMgr->tail)
        (*barMgr->tail)->next = barMgr->pool;
    else
        *barMgr->tail = barMgr->pool;
    barMgr->pool = barMgr->head;
    barMgr->head = NULL;
    barMgr->tail = &barMgr->head;
}

void destroy_barrier_manager(BarrierManager **barMgr)
{
    Barrier *barrier;
    while ((*barMgr)->head) {
        barrier = (*barMgr)->head;
        (*barMgr)->head = (*barMgr)->head->next;
        destroy_barrier(&barrier);
    }
    while ((*barMgr)->pool) {
        barrier = (*barMgr)->pool;
        (*barMgr)->pool = (*barMgr)->pool->next;
        destroy_barrier(&barrier);
    }
    free(*barMgr);
    *barMgr = NULL;
}

Score *create_score()
{
    Score *score = (Score *) malloc(sizeof(Score));

    reset_score(score);

    return score;
}

void reset_score(Score *score)
{
    score->score = 0;
    score->over = 0;
    score->fn = 0;
    score->fps = 0;
    score->dist = 0;
}

void destroy_score(Score **score)
{
    free(*score);
    *score = NULL;
}


/**********************************************************************
*                             functions                              *
**********************************************************************/
int randint(int start, int end) {
    if (start == end)
        return start;
    if (end > start)
        return start + rand() % (end - start);
    return randint(end, start);
}

void init_game()
{
    // generate a canvas
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    // random seed
    srand(time(0));
}

void new_game(Window *valley, Window *panel, Role *start, BarrierManager *barMgr, Bird *bird, Score *score, Args *args)
{
    // reset roles properties
    reset_score(score);
    reset_bird(bird, 20, 10);
    reset_barrier_manager(barMgr);

    // draw background
    valley->draw_self(valley);
    panel->draw_self(panel);

    // draw roles
    valley->draw_role(valley, (Role *) bird->p);
    valley->draw_role(valley, start);

    // sync pixel to screen
    valley->sync_screen(valley);
    panel->sync_screen(panel);

    while (getchar() != 0x20);

    pthread_t draw_thread;
    int ret = pthread_create(&draw_thread, NULL, &play, (void *)args);

    /* void *ret_val; */
    /* pthread_join(draw_thread, &ret_val); */
}

void update_bird(Bird *bird)
{
    bird->p->y += bird->v;
    bird->v += bird->v > MAX_V? 0: GRAVITY;
    if (bird->v > 0) {
        bird->p->cf = 0;
    }
    else {
        bird->p->cf = 1;
    }
    bird->p->skin = bird->p->frames[bird->p->cf];
}

void update_barriers(Window *win, BarrierManager *barMgr, Score *score)
{
    barMgr->check_barrier(barMgr, win, score);
    Barrier *barrier = barMgr->head;
    while(barrier) {
        barrier->p->x += barrier->vx;
        if (barrier->p->y > win->p->h && barrier->vy > 0)
            barrier->vy = -barrier->vy;
        else if (barrier->p->y < barrier->sep && barrier->vy < 0)
            barrier->vy = -barrier->vy;
        barrier->p->y += barrier->vy;
        barrier = barrier->next;
    }
}

int collision_detect(Window *win, Bird *bird, BarrierManager *barMgr)
{
    // collision detect between bird and borders
    if (bird->p->y <= 0 || bird->p->y + bird->p->h >= win->p->h - 1)
        return 1;

    Barrier *barrier = barMgr->head;
    while (barrier) {
        if (barrier->p->x <= bird->p->x + bird->p->w && barrier->p->x + barrier->p->w >= bird->p->x) {
            if (bird->p->y + bird->p->h >= barrier->p->y)
                return 1;
            if (bird->p->y <= barrier->p->y - barrier->sep)
                return 1;
        }
        barrier = barrier->next;
    }
    return 0;
}

void loop(Bird *bird, Score *score)
{
    char key;
    while (1) {
        key = getchar();
        if (key == 0x20) {
            if (score->over)
                return;
            else
                bird->v = MIN_V;
        }
        usleep(interval);
    }
}

void *play(void *_args)
{
    Args *args = (Args *) _args;
    Window *valley = args->valley;
    Window *panel = args->panel;
    Bird *bird = args->bird;
    BarrierManager *barMgr = args->barMgr;
    Score *score = args->score;

    while (1) {
        // collision detect
        if (collision_detect(valley, bird, barMgr)) {
            score->over = 1;
            int x = (valley->p->w - OVER_W) >> 1;
            int y = (valley->p->h - OVER_H) >> 1;
            Role *gameover = create_role(x, y, OVER_W, OVER_H, "gameover.ascii");
            valley->draw_role(valley, gameover);
            valley->sync_screen(valley);
            return NULL;
        }

        // update roles in valley and draw it to screen
        update_bird(bird);
        update_barriers(valley, barMgr, score);
        valley->draw_self(valley);
        valley->draw_role(valley, (Role *) bird->p);
        Barrier *barrier = barMgr->head;
        while (barrier) {
            valley->draw_role(valley, barrier->p);
            barrier->p->y -= barrier->p->h + barrier->sep;
            valley->draw_role(valley, barrier->p);
            barrier->p->y += barrier->p->h + barrier->sep;
            barrier = barrier->next;
        }
        valley->sync_screen(valley);

        // update score
        score->fn++;
        score->dist -= barrier? barrier->vx: BAR_VX;

        // update panel
        panel->draw_self(panel);

        char buff[20] = {0};
        sprintf(buff, "Score:    %d", score->score);
        panel->draw_string(panel, 2, 4, buff);
        sprintf(buff, "Distance: %.1fm", score->dist);
        panel->draw_string(panel, 2, 5, buff);
        sprintf(buff, "FPS:      %d", score->fps);
        panel->draw_string(panel, 2, 6, buff);

        panel->sync_screen(panel);

        usleep(interval);
    }
    return NULL;
}

void *count(void *_args)
{
    Args *args = (Args *) _args;
    Window *panel = args->panel;
    Score *score = args->score;

    unsigned int fn;
    char buff[20] = {0};
    while(1) {
        fn = score->fn;
        sleep(1);
        score->fps = score->fn - fn;;
        if (score->fn > 1000000)
            score->fn = 0;
    }
}

int main()
{
    init_game();

    Window *valley = create_window(0, 0, VALLEY_W, VALLEY_H, "valley.ascii");
    Window *panel = create_window(0, VALLEY_H, PANEL_W, PANEL_H, "panel.ascii");
    Role *start = create_role((VALLEY_W - START_W) >> 1, (VALLEY_H - START_H) >> 1, START_W, START_H, "start.ascii");
    Bird *bird = create_bird(20, 10, 3, 2, "bird.ascii");
    BarrierManager *barMgr = create_barrier_manager();
    Score *score = create_score();
    Args args = {valley, panel, bird, barMgr, score};

    pthread_t count_thread;
    pthread_create(&count_thread, NULL, &count, (void *)&args);
    while (1) {
        new_game(valley, panel, start, barMgr, bird, score, &args);
        loop(bird, score);
    }

    destroy_window(&valley);
    destroy_window(&panel);
    destroy_role(&start);
    destroy_bird(&bird);
    destroy_barrier_manager(&barMgr);
    destroy_score(&score);
    return 0;
}
