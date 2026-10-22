#include <nds.h>
#include "dreamland_mesh.h"
#include "dreamland_render.h"

static int textures[48], background;

static int size_enum(unsigned n)
{
    int result=0;
    for(unsigned s=8;s<n;s*=2)++result;
    return result;
}

int dreamland_render_init(void)
{
    if(dream_texture_count>48)return 0;
    /* A+B = 256 KiB of texture slots. E is explicitly dedicated to palettes;
     * C remains the sub-screen console bank. No DSi memory is used. */
    vramSetBankB(VRAM_B_TEXTURE);
    vramSetBankE(VRAM_E_TEX_PALETTE);
    if(!glGenTextures(dream_texture_count,textures))return 0;
    for(unsigned i=0;i<dream_texture_count;++i) {
        const DreamTexture *t=&dream_textures[i];
        glBindTexture(0,textures[i]);
        if(!glTexImage2D(0,0,GL_RGB16,size_enum(t->width),size_enum(t->height),0,t->param,t->pixels))return 0;
        glColorTableEXT(0,0,16,0,0,t->palette);
    }
    if(!glGenTextures(1,&background))return 0;
    glBindTexture(0,background);
    if(!glTexImage2D(0,0,GL_RGB256,TEXTURE_SIZE_256,TEXTURE_SIZE_256,0,0,dream_wallpaper))return 0;
    glColorTableEXT(0,0,256,0,0,dream_wallpaper_palette);
    return 1;
}

void dreamland_render_background(float left,float right,float bottom,float top)
{
    glBindTexture(0,background);
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(62));
    glColor(RGB15(31,31,31));
    /* VTX_16 is signed 4.12. Use a unit quad and matrix scaling so extreme
     * camera expansion cannot wrap background vertices beyond +/-8. */
    glPushMatrix();
    glTranslatef32(floattof32((left+right)*.5f),floattof32((bottom+top)*.5f),floattof32(-15));
    glScalef32(floattof32((right-left)*.5f),floattof32((top-bottom)*.5f),inttof32(1));
    glBegin(GL_QUADS);
    glTexCoord2t16(0,192<<4);glVertex3v16(-4096,-4096,0);
    glTexCoord2t16(256<<4,192<<4);glVertex3v16(4096,-4096,0);
    glTexCoord2t16(256<<4,0);glVertex3v16(4096,4096,0);
    glTexCoord2t16(0,0);glVertex3v16(-4096,4096,0);
    glEnd();
    glPopMatrix(1);
}

void dreamland_render_draw(void)
{
    glPolyFmt(POLY_ALPHA(31)|POLY_CULL_NONE|POLY_ID(1));
    int previous=-2;
    for(unsigned i=0;i<dream_vertex_count;i+=3) {
        int texture=dream_tri_textures[i/3];
        if(texture!=previous) {
            if(previous!=-2)glEnd();
            glBindTexture(0,texture<0?0:textures[texture]);previous=texture;
            glBegin(GL_TRIANGLE);
        }
        for(unsigned j=0;j<3;++j) {
            const DreamVertex *v=&dream_vertices[i+j];
            glColor(v->color);glTexCoord2t16(v->u,v->v);glVertex3v16(v->x,v->y,v->z);
        }
    }
    if(previous!=-2)glEnd();
}
