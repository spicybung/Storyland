#pragma once

// Include after the platform OpenGL declarations. The caller owns the context.
struct StorylandFrameBlur {
    GLuint texture=0;
    GLsizei width=0,height=0;
    bool ready=false;

    void release() {
        if(texture)glDeleteTextures(1,&texture);
        *this={};
    }

    void draw(GLsizei w,GLsizei h,unsigned opacity,GLint x=0,GLint y=0) {
        if(!opacity||w<=0||h<=0) {ready=false;return;}
        const GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST);
        GLint previousTexture=0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousTexture);
        glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT|GL_TEXTURE_BIT);
        glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
        glDisable(GL_ALPHA_TEST);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
        if(!texture)glGenTextures(1,&texture);
        glBindTexture(GL_TEXTURE_2D,texture);
        if(width!=w||height!=h) {
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
            width=w;height=h;ready=false;
        }
        if(ready) {
            glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
            glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1,1,1,float(opacity)/255.0f);
            glBegin(GL_TRIANGLE_STRIP);
            glTexCoord2f(0,0);glVertex2f(-1,-1);
            glTexCoord2f(1,0);glVertex2f(1,-1);
            glTexCoord2f(0,1);glVertex2f(-1,1);
            glTexCoord2f(1,1);glVertex2f(1,1);
            glEnd();
            glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
        }
        // Save the blended frame before the HUD gets drawn over it.
        glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,x,y,w,h);
        ready=true;
        glPopAttrib();
        glBindTexture(GL_TEXTURE_2D,GLuint(previousTexture));
        if(scissor)glEnable(GL_SCISSOR_TEST);
    }
};
