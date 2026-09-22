#include <math.h>
#include <stdio.h>
#include <tamtypes.h>

#include "GL/gl.h"
#include "GL/glut.h"
#include "GL/ps2gl.h"

#define VU1_DATA_QWORDS 1024
#define VU1_DATA ((volatile u32 *)0x1100c000)
#define FRAME_COUNT 12

static GLfloat l0_position[] = { -1.0f, 0.9f, 0.9f, 0.0f };
static GLfloat l1_position[] = { -2.0f, -1.0f, 1.0f, 0.0f };

static void perspective(float fov, float aspect, float near_clip, float far_clip)
{
    float w, h;
    fov *= 3.141592654f / 180.0f;
    h = 2.0f * near_clip * (float)tanf(fov / 2.0f);
    w = h * aspect;
    glFrustum(-w / 2.0f, w / 2.0f, -h / 2.0f, h / 2.0f, near_clip, far_clip);
}

static void init_lights(void)
{
    GLfloat ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat l0_diffuse[] = { 1.0f, 1.0f, 1.0f, 0.0f };
    GLfloat l1_diffuse[] = { 0.6f, 0.2f, 0.2f, 0.0f };
    GLfloat black[] = { 0.0f, 0.0f, 0.0f, 0.0f };

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_RESCALE_NORMAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, l0_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, l0_diffuse);

    glLightfv(GL_LIGHT1, GL_AMBIENT, black);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, l1_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, black);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.005f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.0f);
}

static void draw_cube(void)
{
    static const float points[8][3] = {
        { -1, 1, -1 }, { -1, 1, 1 }, { 1, 1, 1 }, { 1, 1, -1 },
        { -1, -1, -1 }, { -1, -1, 1 }, { 1, -1, 1 }, { 1, -1, -1 }
    };
    static const float material_diff_amb[] = { 0.5f, 0.5f, 0.5f, 0.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material_diff_amb);
    glBegin(GL_QUADS);

    glNormal3f(0, 1, 0);
    glVertex3fv(points[0]); glVertex3fv(points[1]); glVertex3fv(points[2]); glVertex3fv(points[3]);

    glNormal3f(0, -1, 0);
    glVertex3fv(points[7]); glVertex3fv(points[6]); glVertex3fv(points[5]); glVertex3fv(points[4]);

    glNormal3f(-1, 0, 0);
    glVertex3fv(points[0]); glVertex3fv(points[4]); glVertex3fv(points[5]); glVertex3fv(points[1]);

    glNormal3f(1, 0, 0);
    glVertex3fv(points[2]); glVertex3fv(points[6]); glVertex3fv(points[7]); glVertex3fv(points[3]);

    glNormal3f(0, 0, 1);
    glVertex3fv(points[1]); glVertex3fv(points[5]); glVertex3fv(points[6]); glVertex3fv(points[2]);

    glNormal3f(0, 0, -1);
    glVertex3fv(points[3]); glVertex3fv(points[7]); glVertex3fv(points[4]); glVertex3fv(points[0]);

    glEnd();
}

static u32 fnv_word(u32 hash, u32 word)
{
    hash ^= word;
    return hash * 16777619u;
}

/*
 * ps2gl qwords 4..8 contain EE pointers / transport-header state whose exact
 * values depend on the linked ELF layout. They are not renderer results.
 * Compare qwords 0..3 and 9..1023, which cover the stable VU1 state and output
 * buffers exercised by this fixture.
 */
static u32 semantic_vu1_hash(void)
{
    u32 hash = 2166136261u;
    int qw;
    int lane;

    for (qw = 0; qw < VU1_DATA_QWORDS; ++qw) {
        if (qw >= 4 && qw <= 8)
            continue;
        for (lane = 0; lane < 4; ++lane)
            hash = fnv_word(hash, VU1_DATA[qw * 4 + lane]);
    }
    return hash;
}

int main(int argc, char **argv)
{
    int frame;

    glutInit(&argc, argv);
    glClearColor(0.3f, 0.3f, 0.4f, 0.0f);
    init_lights();

    glViewport(0, 0, 640, 448);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    perspective(40.0f, 640.0f / 448.0f, 1.0f, 4000.0f);

    for (frame = 0; frame < FRAME_COUNT; ++frame) {
        pglBeginGeometry();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0, 0, -5.0f);
        glRotatef((float)(frame * 7), 0, 1, 0);
        glRotatef((float)(frame * 3), -1, 0, 0);
        glLightfv(GL_LIGHT0, GL_POSITION, l0_position);
        glLightfv(GL_LIGHT1, GL_POSITION, l1_position);
        glRotatef((float)(frame * 11), 0, 1, 0);

        draw_cube();
        glFlush();
        pglEndGeometry();

        pglSwapBuffers();
        pglRenderGeometry();
        pglFinishRenderingGeometry(PGL_DONT_FORCE_IMMEDIATE_STOP);

        printf("PS2GL_GENERAL_QUAD_FRAME frame=%d renderer=%s semantic=%08x\n",
               frame, pglGetCurRendererName(), semantic_vu1_hash());
    }

    printf("VU1_DIFFERENTIAL_PASS ps2gl_general_quad_frames=%d\n", FRAME_COUNT);
    return 0;
}
