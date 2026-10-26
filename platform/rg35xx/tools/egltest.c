// GPU plan G2 smoke test: EGL + OpenGL ES 2.0 on the PowerVR SGX544, drawn
// straight to the framebuffer by the window system in /etc/powervr.ini.
// Prints the EGL/GL strings, then spins a colored triangle and prints the
// frame rate every second. Needs pvrsrvkm.ko loaded and `pvrsrvctl --start`.
// Usage: egltest [seconds] (default 10)

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char *vertex_src =
    "attribute vec2 pos;\n"
    "attribute vec3 color;\n"
    "uniform float angle;\n"
    "varying vec3 v_color;\n"
    "void main() {\n"
    "    float c = cos(angle), s = sin(angle);\n"
    "    gl_Position = vec4(c * pos.x - s * pos.y, s * pos.x + c * pos.y, 0.0, 1.0);\n"
    "    v_color = color;\n"
    "}\n";

static const char *fragment_src =
    "precision mediump float;\n"
    "varying vec3 v_color;\n"
    "void main() { gl_FragColor = vec4(v_color, 1.0); }\n";

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static GLuint compile(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
    GLint ok;
    char log[512];

    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "shader: %s\n", log);
        exit(1);
    }
    return shader;
}

int main(int argc, char *argv[])
{
    int seconds = argc > 1 ? atoi(argv[1]) : 10;
    EGLint major, minor, nconfigs, width, height;
    EGLConfig config;
    static const EGLint config_attribs[] = {
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE};
    static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    static const GLfloat vertices[] = {
        0.0f, 0.8f, 1.0f, 0.0f, 0.0f,
        -0.7f, -0.5f, 0.0f, 1.0f, 0.0f,
        0.7f, -0.5f, 0.0f, 0.0f, 1.0f};

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) {
        fprintf(stderr, "eglInitialize failed: 0x%x\n", eglGetError());
        return 1;
    }
    printf("EGL %d.%d, vendor: %s, version: %s\n", major, minor,
           eglQueryString(display, EGL_VENDOR), eglQueryString(display, EGL_VERSION));

    if (!eglChooseConfig(display, config_attribs, &config, 1, &nconfigs) || nconfigs < 1) {
        fprintf(stderr, "eglChooseConfig failed: 0x%x\n", eglGetError());
        return 1;
    }
    // The framebuffer window systems take no native window
    EGLSurface surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)0, NULL);
    if (surface == EGL_NO_SURFACE) {
        fprintf(stderr, "eglCreateWindowSurface failed: 0x%x\n", eglGetError());
        return 1;
    }
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) {
        fprintf(stderr, "eglCreateContext/MakeCurrent failed: 0x%x\n", eglGetError());
        return 1;
    }
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);
    printf("surface %dx%d\nGL vendor: %s\nGL renderer: %s\nGL version: %s\n", width, height,
           glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

    GLuint program = glCreateProgram();
    glAttachShader(program, compile(GL_VERTEX_SHADER, vertex_src));
    glAttachShader(program, compile(GL_FRAGMENT_SHADER, fragment_src));
    glBindAttribLocation(program, 0, "pos");
    glBindAttribLocation(program, 1, "color");
    glLinkProgram(program);
    glUseProgram(program);
    GLint angle = glGetUniformLocation(program, "angle");

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices + 2);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glViewport(0, 0, width, height);

    double start = now(), last = start;
    int frames = 0;
    while (now() - start < seconds) {
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform1f(angle, (float)(now() - start));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        eglSwapBuffers(display, surface);
        frames++;
        if (now() - last >= 1.0) {
            printf("%.1f fps\n", frames / (now() - last));
            frames = 0;
            last = now();
        }
    }

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    return 0;
}
