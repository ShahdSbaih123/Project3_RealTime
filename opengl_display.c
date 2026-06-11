#include <GL/glut.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "opengl_display.h"
#include "logger.h"


#define BG_R      0.12f  
#define BG_G      0.07f
#define BG_B      0.13f

#define TITLE_R   0.78f 
#define TITLE_G   0.25f
#define TITLE_B   0.55f

#define ACCENT_R  0.95f  
#define ACCENT_G  0.40f
#define ACCENT_B  0.70f

#define ROW_A_R   0.20f 
#define ROW_A_G   0.10f
#define ROW_A_B   0.20f

#define ROW_B_R   0.16f 
#define ROW_B_G   0.08f
#define ROW_B_B   0.17f

#define TEXT_R    1.00f  
#define TEXT_G    0.92f
#define TEXT_B    0.96f

#define SUB_R     0.90f  
#define SUB_G     0.70f
#define SUB_B     0.95f

#define ERR_R     1.00f 
#define ERR_G     0.35f
#define ERR_B     0.55f

#define BADGE_ON_R  0.85f  
#define BADGE_ON_G  0.20f
#define BADGE_ON_B  0.55f

#define BADGE_OFF_R 0.30f  
#define BADGE_OFF_G 0.18f
#define BADGE_OFF_B 0.30f

#define BAR_TRACK_R 0.22f  
#define BAR_TRACK_G 0.10f
#define BAR_TRACK_B 0.22f

#define BAR_FILL_R  0.85f 
#define BAR_FILL_G  0.25f
#define BAR_FILL_B  0.60f

#define BAR_DONE_R  0.50f  
#define BAR_DONE_G  0.20f
#define BAR_DONE_B  0.85f

#define FOOT_R    0.20f  
#define FOOT_G    0.10f
#define FOOT_B    0.20f

/* ═══════════════════════════════════════════════════════════
 * Shared draw helpers
 * ═══════════════════════════════════════════════════════════ */

static void draw_string(float x, float y, const char *s, void *font)
{
    glRasterPos2f(x, y);
    for (; *s; s++)
        glutBitmapCharacter(font, (unsigned char)*s);
}

static void fill_rect(float x0, float y0, float x1, float y1)
{
    glBegin(GL_QUADS);
        glVertex2f(x0, y0); glVertex2f(x1, y0);
        glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
}

static void set_2d(void)
{
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
}

/* ═══════════════════════════════════════════════════════════
 * Log ring buffer  (server events)
 * ═══════════════════════════════════════════════════════════ */

#define LOG_LINES  8
#define LOG_WIDTH  128

static int  s_win        = -1;
static char s_ring[LOG_LINES][LOG_WIDTH];
static int  s_head       = 0;
static int  s_count      = 0;
static pthread_mutex_t s_ring_mtx = PTHREAD_MUTEX_INITIALIZER;

static void poll_logger(void)
{
    static char seen[LOG_WIDTH] = "";
    char latest[LOG_WIDTH];
    logger_get_last_line(latest, LOG_WIDTH);
    if (strncmp(latest, seen, LOG_WIDTH - 1) == 0) return;
    strncpy(seen, latest, LOG_WIDTH - 1);
    pthread_mutex_lock(&s_ring_mtx);
    strncpy(s_ring[s_head], latest, LOG_WIDTH - 1);
    s_ring[s_head][LOG_WIDTH - 1] = '\0';
    s_head = (s_head + 1) % LOG_LINES;
    if (s_count < LOG_LINES) s_count++;
    pthread_mutex_unlock(&s_ring_mtx);
}

/* ═══════════════════════════════════════════════════════════
 * Client progress state
 * ═══════════════════════════════════════════════════════════ */

static int  s_progress  = 0;
static char s_status[256] = "Waiting for client...";
static pthread_mutex_t s_cli_mtx = PTHREAD_MUTEX_INITIALIZER;

void opengl_client_set_progress(int pct)
{
    pthread_mutex_lock(&s_cli_mtx);
    s_progress = (pct < 0) ? 0 : (pct > 100) ? 100 : pct;
    pthread_mutex_unlock(&s_cli_mtx);
}

void opengl_client_set_status(const char *msg)
{
    if (!msg) return;
    pthread_mutex_lock(&s_cli_mtx);
    strncpy(s_status, msg, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
    pthread_mutex_unlock(&s_cli_mtx);
}

/* ═══════════════════════════════════════════════════════════
 * Main display callback  (single unified window)
 * ═══════════════════════════════════════════════════════════ */

static void display_cb(void)
{
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    float fw = (float)w, fh = (float)h;

    glClearColor(BG_R, BG_G, BG_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    set_2d();

    /* ── Title bar ───────────────────────────────────────── */
    glColor3f(TITLE_R, TITLE_G, TITLE_B);
    fill_rect(0, fh - 42, fw, fh);

    glColor3f(TEXT_R, TEXT_G, TEXT_B);
    draw_string(14, fh - 28, "  Update Server Monitor  ✦  ENCS4330",
                GLUT_BITMAP_HELVETICA_18);

    /* ── Connected-clients badge ─────────────────────────── */
    int cc = logger_get_client_count();
    glColor3f(cc > 0 ? BADGE_ON_R  : BADGE_OFF_R,
              cc > 0 ? BADGE_ON_G  : BADGE_OFF_G,
              cc > 0 ? BADGE_ON_B  : BADGE_OFF_B);
    fill_rect(fw - 176, fh - 38, fw - 6, fh - 6);

    glColor3f(TEXT_R, TEXT_G, TEXT_B);
    char badge[48];
    snprintf(badge, sizeof(badge), "  Clients: %d  ", cc);
    draw_string(fw - 172, fh - 28, badge, GLUT_BITMAP_HELVETICA_12);

    /* ── Section label: Recent Events ───────────────────── */
    glColor3f(SUB_R, SUB_G, SUB_B);
    draw_string(14, fh - 62, "Recent Events", GLUT_BITMAP_HELVETICA_12);

    /* pink divider */
    glColor3f(ACCENT_R, ACCENT_G, ACCENT_B);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(14,       fh - 68);
        glVertex2f(fw - 14, fh - 68);
    glEnd();

    /* ── Log lines ───────────────────────────────────────── */
    pthread_mutex_lock(&s_ring_mtx);
    int lh = 19;
    float y = fh - 88;
    for (int i = 0; i < s_count && y > 140; i++) {
        int idx = ((s_head - 1 - i) + LOG_LINES) % LOG_LINES;

        /* row background */
        glColor3f(i % 2 == 0 ? ROW_A_R : ROW_B_R,
                  i % 2 == 0 ? ROW_A_G : ROW_B_G,
                  i % 2 == 0 ? ROW_A_B : ROW_B_B);
        fill_rect(10, y - 4, fw - 10, y + 14);

        /* text colour: error = coral, normal = bright white */
        if (strstr(s_ring[idx], "ERROR"))
            glColor3f(ERR_R, ERR_G, ERR_B);
        else
            glColor3f(TEXT_R, TEXT_G, TEXT_B);

        draw_string(16, y, s_ring[idx], GLUT_BITMAP_8_BY_13);
        y -= lh;
    }
    pthread_mutex_unlock(&s_ring_mtx);

    /* ── Client progress section ─────────────────────────── */

    /* section divider */
    glColor3f(ACCENT_R, ACCENT_G, ACCENT_B);
    glBegin(GL_LINES);
        glVertex2f(14,       140);
        glVertex2f(fw - 14, 140);
    glEnd();

    glColor3f(SUB_R, SUB_G, SUB_B);
    draw_string(14, 124, "Client Progress", GLUT_BITMAP_HELVETICA_12);

    /* status text */
    pthread_mutex_lock(&s_cli_mtx);
    int  pct = s_progress;
    char status[256];
    strncpy(status, s_status, sizeof(status) - 1);
    status[255] = '\0';
    pthread_mutex_unlock(&s_cli_mtx);

    glColor3f(TEXT_R, TEXT_G, TEXT_B);
    draw_string(14, 106, status, GLUT_BITMAP_8_BY_13);

    /* bar track */
    float bx0 = 14, bx1 = fw - 14, by0 = 56, by1 = 84;
    float bw   = bx1 - bx0;

    glColor3f(BAR_TRACK_R, BAR_TRACK_G, BAR_TRACK_B);
    fill_rect(bx0, by0, bx1, by1);

    /* bar fill */
    float fx = bx0 + bw * ((float)pct / 100.0f);
    if (pct >= 100)
        glColor3f(BAR_DONE_R, BAR_DONE_G, BAR_DONE_B);
    else
        glColor3f(BAR_FILL_R, BAR_FILL_G, BAR_FILL_B);
    if (fx > bx0) fill_rect(bx0, by0, fx, by1);

    /* bar border */
    glColor3f(ACCENT_R, ACCENT_G, ACCENT_B);
    glBegin(GL_LINE_LOOP);
        glVertex2f(bx0, by0); glVertex2f(bx1, by0);
        glVertex2f(bx1, by1); glVertex2f(bx0, by1);
    glEnd();

    /* percentage centred on bar */
    char ps[8];
    snprintf(ps, sizeof(ps), "%d%%", pct);
    float cx = (bx0 + bx1) / 2.0f - (float)(strlen(ps) * 7) / 2.0f;
    glColor3f(TEXT_R, TEXT_G, TEXT_B);
    draw_string(cx, (by0 + by1) / 2.0f - 5, ps, GLUT_BITMAP_HELVETICA_12);

    /* ── Footer ──────────────────────────────────────────── */
    glColor3f(FOOT_R, FOOT_G, FOOT_B);
    fill_rect(0, 0, fw, 24);
    glColor3f(SUB_R, SUB_G, SUB_B);
    draw_string(14, 7, "Birzeit University  ✦  Real-Time & Embedded Systems  ✦  2025/2026",
                GLUT_BITMAP_8_BY_13);

    glutSwapBuffers();
}

/* ═══════════════════════════════════════════════════════════
 * Timer & reshape
 * ═══════════════════════════════════════════════════════════ */

static void timer_cb(int val)
{
    (void)val;
    poll_logger();
    glutPostRedisplay();
    glutTimerFunc(400, timer_cb, 0);
}

static void reshape_cb(int w, int h)
{
    glViewport(0, 0, w, h);
}

/* ═══════════════════════════════════════════════════════════
 * Public API  (server side — opens the one shared window)
 * ═══════════════════════════════════════════════════════════ */

void opengl_server_init(int *argc, char **argv)
{
    glutInit(argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(820, 480);
    glutInitWindowPosition(60, 60);
    s_win = glutCreateWindow("Update Server Monitor  ✦  ENCS4330");
    glutDisplayFunc(display_cb);
    glutReshapeFunc(reshape_cb);
    glutTimerFunc(400, timer_cb, 0);
}

void opengl_server_run(void)
{
    glutMainLoop();
}

/* ═══════════════════════════════════════════════════════════
 * Public API  (client side — reuses the same window via the
 * shared logger; no second glutInit / glutCreateWindow needed)
 * ═══════════════════════════════════════════════════════════ */

void opengl_client_init(int *argc, char **argv)
{
    /* Client does NOT create its own window — it just reuses the
       shared state that display_cb() already draws.
       If running standalone (no server window open), we open one. */
    if (s_win == -1) {
        glutInit(argc, argv);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
        glutInitWindowSize(820, 480);
        glutInitWindowPosition(60, 60);
        s_win = glutCreateWindow("Client Update Progress  ✦  ENCS4330");
        glutDisplayFunc(display_cb);
        glutReshapeFunc(reshape_cb);
        glutTimerFunc(400, timer_cb, 0);
    }
    (void)argc; (void)argv;
}

void opengl_client_run(void)
{
    if (s_win != -1)
        glutMainLoop();
}