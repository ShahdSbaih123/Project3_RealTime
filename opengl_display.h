#ifndef OPENGL_DISPLAY_H
#define OPENGL_DISPLAY_H

#ifdef NO_OPENGL
/* Headless / test mode — all GL calls become no-ops */
static inline void opengl_server_init(int *a, char **b) { (void)a;(void)b; }
static inline void opengl_server_run(void)               {}
static inline void opengl_client_init(int *a, char **b)  { (void)a;(void)b; }
static inline void opengl_client_run(void)               {}
static inline void opengl_client_set_progress(int p)     { (void)p; }
static inline void opengl_client_set_status(const char *m){ (void)m; }
#else

/* ── Server monitor ────────────────────────────────────────── */

/**
 * Initialise GLUT and open the server-monitor window.
 * Must be called from the main thread before opengl_server_run().
 */
void opengl_server_init(int *argc, char **argv);

/**
 * Enter the GLUT main loop (blocks forever).
 * Spawn this in a dedicated thread so accept_loop() can run in main.
 */
void opengl_server_run(void);

/* ── Client progress display ───────────────────────────────── */

/**
 * Initialise GLUT and open the client-progress window.
 */
void opengl_client_init(int *argc, char **argv);

/**
 * Enter the GLUT main loop for the client window.
 */
void opengl_client_run(void);

/**
 * Update the download-progress value seen by the client window.
 * @param percent  0–100
 */
void opengl_client_set_progress(int percent);

/**
 * Set the status message shown in the client window.
 */
void opengl_client_set_status(const char *msg);

#endif /* NO_OPENGL */

#endif /* OPENGL_DISPLAY_H */