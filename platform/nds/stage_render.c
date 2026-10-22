#include <nds.h>
#include "stage.h"
#include "stage_render.h"

static void vertex(float x, float y, float z)
{
    glVertex3v16(floattov16(x/400), floattov16(y/400), floattov16(z));
}

static void quad(float l, float r, float bottom, float top, float z, unsigned color)
{
    glColor(color);
    glBegin(GL_QUADS);
    vertex(l,bottom,z); vertex(r,bottom,z); vertex(r,top,z); vertex(l,top,z);
    glEnd();
}

static void ellipse(float x, float y, float rx, float ry, float z, unsigned color)
{
    static const float circle[17][2] = {
        {1,0},{.9238795f,.3826834f},{.7071068f,.7071068f},{.3826834f,.9238795f},
        {0,1},{-.3826834f,.9238795f},{-.7071068f,.7071068f},{-.9238795f,.3826834f},
        {-1,0},{-.9238795f,-.3826834f},{-.7071068f,-.7071068f},{-.3826834f,-.9238795f},
        {0,-1},{.3826834f,-.9238795f},{.7071068f,-.7071068f},{.9238795f,-.3826834f},{1,0}
    };
    glColor(color);
    glBegin(GL_TRIANGLE);
    for (int i = 0; i < 16; ++i) {
        vertex(x,y,z);
        vertex(x+circle[i][0]*rx,y+circle[i][1]*ry,z);
        vertex(x+circle[i+1][0]*rx,y+circle[i+1][1]*ry,z);
    }
    glEnd();
}

void stage_render_draw(void)
{
    /* Original training scenery drawn by DS geometry hardware. All playable
     * surfaces below come from the same table used by the collision system. */
    glBindTexture(0, 0);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
    ellipse(-1400,1600,300,85,-2,RGB15(29,31,31));
    ellipse(1150,1850,350,100,-2,RGB15(29,31,31));
    ellipse(-1800,300,700,500,-1.7,RGB15(13,24,22));
    ellipse(1700,250,700,430,-1.7,RGB15(15,26,23));
    quad(-50,100,0,1200,-1.3,RGB15(16,10,5));
    quad(-10,35,30,1150,-1.29,RGB15(21,14,7));
    ellipse(-240,1150,420,350,-1.35,RGB15(7,20,10));
    ellipse(240,1300,480,330,-1.34,RGB15(9,24,10));
    ellipse(-80,1460,430,300,-1.33,RGB15(12,27,12));
    quad(-900,900,-170,0,-.25,RGB15(18,12,7));
    glColor(RGB15(13,8,5));
    glBegin(GL_TRIANGLE);
    vertex(-900,-170,-.25); vertex(900,-170,-.25); vertex(0,-520,-.25);
    glEnd();
    for (int i = 0; i < 9; ++i)
        quad(-820+i*190,-740+i*190,-130,-80,-.24,RGB15(22,16,9));
    unsigned count;
    const StagePlatform *platforms = stage_platforms(STAGE_GARDEN,&count);
    for (unsigned i = 0; i < count; ++i) {
        const StagePlatform *p = &platforms[i];
        quad(p->left,p->right,p->height-(i ? 45 : 32),p->height,-.10,RGB15(11,24,8));
        quad(p->left,p->right,p->height-10,p->height, -.09,RGB15(21,30,12));
        if (i) quad(p->left+15,p->right-15,p->height-70,p->height-45,-.11,RGB15(20,13,6));
    }
}
