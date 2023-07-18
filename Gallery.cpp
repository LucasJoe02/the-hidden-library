//  ========================================================================
//  COSC363: Computer Graphics (2023);  University of Canterbury.
//
//  FILE NAME: Teapot.cpp
//  See Lab01.pdf for details
//  ========================================================================

#include <cmath>
#include <iostream>
#include <fstream>
#include <climits>
#include "loadTGA.h"
#include "FastNoiseLite.h"
#include <GL/freeglut.h>
#include <glm/vec3.hpp>
#include <glm/ext/matrix_transform.hpp>
using namespace std;

const int FLOOR_SIZE = 25;
const int TILE_SIZE = 4;
const int WATER_SIZE = 89;
const float AMES_SPEED = 0.5f;
const float LIGHT_SPEED = 0.2f;
const float SUB_SPEED = 0.2f;

const float CAMERA_SPEED = 2.0f;
glm::vec3 cameraPos = glm::vec3(0.0f, 15.0f, 30.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float cameraYaw = -90;

float amesAngle = 0;
float lightAngle = 0;
float subAngle = 0;
float light1X = -1;
float light1Y = 2.5;
float light1Z = -2;
float light1Cut = 40;
float light1Exp = 10;
float scanAmount = -0.05;

float ballVelocity = 0;
float ballHeight = 0;
float ballForce = 0.005;
bool ballFalling = false;

float noiseOffset;
FastNoiseLite noise;
GLuint txId[6];   //Texture ids

class Object {
public:
    float *x, *y, *z;					//vertex coordinates
    int *nv, *t1, *t2, *t3, *t4;		//number of vertices and vertex indices of each face
    int nvert, nface;					//total number of vertices and faces
    float angleX = 10.0,  angleY = -20;	//Rotation angles about x, y axes
    float xmin, xmax, ymin, ymax;		//min, max values of  object coordinates
};

Object* pedestal = new Object();
Object* submarine = new Object();

//---- Function to load textures ----
void loadTexture()
{
    glGenTextures(6, txId); 	// Create 1 texture ids

    glBindTexture(GL_TEXTURE_2D, txId[0]);
    loadTGA("scanimation.tga");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, txId[1]);
    loadTGA("lines.tga");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, txId[2]);
    loadTGA("tiles.tga");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, txId[3]);
    loadTGA("elephant.tga");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, txId[4]);
    loadTGA("squares.tga");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, txId[5]);
    loadTGA("cloth.tga");
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
}

//-- Loads mesh data in OFF format    -------------------------------------
void loadMeshFile(const char* fname, Object* object)
{
    ifstream fp_in;
    int ne;

    fp_in.open(fname, ios::in);
    if(!fp_in.is_open())
    {
        cout << "Error opening mesh file" << endl;
        exit(1);
    }

    fp_in.ignore(INT_MAX, '\n');				//ignore first line
    fp_in >> object->nvert >> object->nface >> ne;			    // read number of vertices, polygons, edges (not used)

    object->x = new float[object->nvert];                        //create arrays
    object->y = new float[object->nvert];
    object->z = new float[object->nvert];

    object->nv = new int[object->nface];
    object->t1 = new int[object->nface];
    object->t2 = new int[object->nface];
    object->t3 = new int[object->nface];
    object->t4 = new int[object->nface];

    for(int i=0; i < object->nvert; i++)                         //read vertex list
        fp_in >> object->x[i] >> object->y[i] >> object->z[i];

    for(int i=0; i < object->nface; i++)                         //read polygon list
    {
        fp_in >> object->nv[i] >> object->t1[i] >> object->t2[i] >> object->t3[i];
        if (object->nv[i] == 4)
            fp_in >> object->t4[i];
        else
            object->t4[i] = -1;
    }

    fp_in.close();
}

void normal(int indx, Object* object)
{
    float x1 = object->x[object->t1[indx]], x2 = object->x[object->t2[indx]], x3 = object->x[object->t3[indx]];
    float y1 = object->y[object->t1[indx]], y2 = object->y[object->t2[indx]], y3 = object->y[object->t3[indx]];
    float z1 = object->z[object->t1[indx]], z2 = object->z[object->t2[indx]], z3 = object->z[object->t3[indx]];
    float nx, ny, nz;
    nx = y1*(z2-z3) + y2*(z3-z1) + y3*(z1-z2);
    ny = z1*(x2-x3) + z2*(x3-x1) + z3*(x1-x2);
    nz = x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2);
    glNormal3f(nx, ny, nz);
}

void drawMesh(Object* object)
{
    for(int indx = 0; indx < object->nface; indx++)		//draw each face
    {
        normal(indx, object);
        if (object->nv[indx] == 3)	glBegin(GL_TRIANGLES);
        else				glBegin(GL_QUADS);
            glVertex3d(object->x[object->t1[indx]], object->y[object->t1[indx]], object->z[object->t1[indx]]);
            glVertex3d(object->x[object->t2[indx]], object->y[object->t2[indx]], object->z[object->t2[indx]]);
            glVertex3d(object->x[object->t3[indx]], object->y[object->t3[indx]], object->z[object->t3[indx]]);
            if(object->nv[indx]==4)
              glVertex3d(object->x[object->t4[indx]], object->y[object->t4[indx]], object->z[object->t4[indx]]);
        glEnd();
    }
}

void special(int key, int x, int y)
{
    if(key == GLUT_KEY_RIGHT) cameraYaw += CAMERA_SPEED;
    else if(key == GLUT_KEY_LEFT) cameraYaw -= CAMERA_SPEED;
    else if(key == GLUT_KEY_UP) cameraPos += CAMERA_SPEED * cameraFront;
    else if(key == GLUT_KEY_DOWN) cameraPos -= CAMERA_SPEED * cameraFront;
 }

void keyboard(unsigned char key, int x, int y)
{
    switch(key){
        case '0':
            cameraPos = glm::vec3(49,15,48);
            cameraYaw = 235;
            break;
        case '1':
            cameraPos = glm::vec3(0,15,-25);
            cameraYaw = -90;
            break;
        case '2':
            cameraPos = glm::vec3(-25,15,0);
            cameraYaw = 180;
            break;
        case '3':
            cameraPos = glm::vec3(25,15,0);
            cameraYaw = 0;
            break;
    }
 }

void animationTimer(int value)
{
    amesAngle += AMES_SPEED;
    if(amesAngle >= 360){amesAngle = 0;}
    lightAngle += LIGHT_SPEED;
    if(lightAngle >= 360){lightAngle = 0;}
    subAngle += SUB_SPEED;
    if(subAngle >= 360){subAngle = 0;}
    noiseOffset += 0.5;
    if(value%10 == 0){scanAmount*=-1;}

    if (ballHeight <= 1.5) {
        ballFalling = false;
    } else if (ballHeight >= 16.5) {
        ballFalling = true;
    }
    if (ballFalling) {
        ballVelocity -= ballForce;
    } else {
        ballVelocity = 0.1;
    }
    ballHeight += ballVelocity;

    value++;
    glutPostRedisplay();
    glutTimerFunc(4,animationTimer,value);
}

//--Draws a grid of quads on the floor plane -------------------------------
void drawWalls() {
    glPushMatrix();
        glColor3f(1,1,1);
        glBindTexture(GL_TEXTURE_2D, txId[2]);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
            glNormal3f(0,0,1);
            glTexCoord2f(1,0);
            glVertex3f(-FLOOR_SIZE/2*TILE_SIZE,0,-FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(0,0);
            glVertex3f(FLOOR_SIZE/2*TILE_SIZE,0,-FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(0,0.75);
            glVertex3f(FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE,-FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(1,0.75);
            glVertex3f(-FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE,-FLOOR_SIZE/2*TILE_SIZE);

            glNormal3f(1,0,0);
            glTexCoord2f(0,0);
            glVertex3f(-FLOOR_SIZE/2*TILE_SIZE,0,FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(1,0);
            glVertex3f(-FLOOR_SIZE/2*TILE_SIZE,0,-FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(1,0.75);
            glVertex3f(-FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE,-FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(0,0.75);
            glVertex3f(-FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE);

            glNormal3f(0,0,-1);
            glTexCoord2f(0,0);
            glVertex3f(-FLOOR_SIZE/2*TILE_SIZE,0,FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(1,0);
            glVertex3f(FLOOR_SIZE/2*TILE_SIZE,0,FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(1,0.75);
            glVertex3f(FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(0,0.75);
            glVertex3f(-FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE);

            glNormal3f(-1,0,0);
            glTexCoord2f(0,0);
            glVertex3f(FLOOR_SIZE/2*TILE_SIZE,0,-FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(1,0);
            glVertex3f(FLOOR_SIZE/2*TILE_SIZE,0,FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(1,0.75);
            glVertex3f(FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE);
            glTexCoord2f(0,0.75);
            glVertex3f(FLOOR_SIZE/2*TILE_SIZE,FLOOR_SIZE/2*TILE_SIZE,-FLOOR_SIZE/2*TILE_SIZE);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

//--Draws a grid of quads on the floor plane -------------------------------
void drawFloor()
{
    glPushMatrix();
        glColor3f(0.5, 0.125,  0.45);			//Floor colour
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
        glNormal3f(0, 1, 0);
        glBegin(GL_QUADS);
        for(int x = -FLOOR_SIZE/2; x < FLOOR_SIZE/2; x++)
        {
            for(int z = -FLOOR_SIZE/2; z < FLOOR_SIZE/2; z++)
            {
                if ((x+z)%2) {
                    glColor3f(1, 0.99,  0.82);
                } else {
                    glColor3f(0.24, 0.21,  0.21);
                }
                glVertex3f(x*TILE_SIZE, 0, z*TILE_SIZE);
                glVertex3f(x*TILE_SIZE, 0, (z+1)*TILE_SIZE);
                glVertex3f((x+1)*TILE_SIZE, 0, (z+1)*TILE_SIZE);
                glVertex3f((x+1)*TILE_SIZE, 0, z*TILE_SIZE);
            }
        }
        glEnd();
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    glPopMatrix();
}

//--Draws an Ames window----------------------------------------------------
void drawAmesWindow(bool flipped)
{
    glPushMatrix();
        glScalef(0.2, 0.2, 0.2);
        if (flipped) {
            glScalef(-1,1,1);
            glRotatef(180,0,1,0);
            glRotatef(amesAngle*-1,0,1,0);
            glTranslatef(0,0,0.001);
        } else {
            glRotatef(amesAngle,0,1,0);
        }
        glTranslatef(-11.5,0,0);
        glBegin(GL_QUADS);
            //window
            glColor3f(1.0, 1.0,  1.0);

            //middle
            glVertex3f(0,-0.4,0);
            glVertex3f(19.8,-0.8,0);
            glVertex3f(19.8,0.8,0);
            glVertex3f(0,0.4,0);

            //top
            glVertex3f(0,3,0);
            glVertex3f(19.8,5.8,0);
            glVertex3f(19.8,7.4,0);
            glVertex3f(0,3.9,0);

            //bottom
            glVertex3f(0,-3,0);
            glVertex3f(19.8,-5.8,0);
            glVertex3f(19.8,-7.4,0);
            glVertex3f(0,-3.9,0);

            //first
            glVertex3f(0,-3.8,0);
            glVertex3f(0.7,-3.9,0);
            glVertex3f(0.7,3.9,0);
            glVertex3f(0,3.8,0);

            //second
            glVertex3f(5.3,-3.75,0);
            glVertex3f(6.2,-3.87,0);
            glVertex3f(6.2,-0.5,0);
            glVertex3f(5.3,-0.4,0);

            glVertex3f(5.3,0.4,0);
            glVertex3f(6.2,0.5,0);
            glVertex3f(6.2,3.87,0);
            glVertex3f(5.3,3.75,0);

            //third
            glVertex3f(11.7,-4.7,0);
            glVertex3f(12.9,-4.8,0);
            glVertex3f(12.9,-0.65,0);
            glVertex3f(11.7,-0.55,0);

            glVertex3f(11.7,0.55,0);
            glVertex3f(12.9,0.65,0);
            glVertex3f(12.9,4.8,0);
            glVertex3f(11.7,4.7,0);

            //fourth
            glVertex3f(19.7,-7.4,0);
            glVertex3f(21.4,-7.7,0);
            glVertex3f(21.4,7.7,0);
            glVertex3f(19.7,7.4,0);

            //Shading
            glColor3f(0.0, 0.0,0.545);
            //first
            glVertex3f(0.7,-3.1,0);
            glVertex3f(1.6,-2.8,0);
            glVertex3f(1.6,-0.4,0);
            glVertex3f(0.7,-0.4,0);

            glVertex3f(0.7,3.1,0);
            glVertex3f(1.6,2.8,0);
            glVertex3f(1.6,0.4,0);
            glVertex3f(0.7,0.4,0);

            //second
            glVertex3f(0.7,-3.1,0);
            glVertex3f(5.3,-3.7,0);
            glVertex3f(5.3,-3.3,0);
            glVertex3f(1.6,-2.8,0);

            glVertex3f(0.7,3.1,0);
            glVertex3f(5.3,3.7,0);
            glVertex3f(5.3,3.3,0);
            glVertex3f(1.6,2.8,0);

            //third
            glVertex3f(6.2,-3.8,0);
            glVertex3f(7.3,-3.5,0);
            glVertex3f(7.3,-0.5,0);
            glVertex3f(6.2,-0.5,0);

            glVertex3f(6.2,3.8,0);
            glVertex3f(7.3,3.5,0);
            glVertex3f(7.3,0.5,0);
            glVertex3f(6.2,0.5,0);

            //fourth
            glVertex3f(6.2,-3.8,0);
            glVertex3f(11.7,-4.7,0);
            glVertex3f(11.7,-4.1,0);
            glVertex3f(7.3,-3.5,0);

            glVertex3f(6.2,3.8,0);
            glVertex3f(11.7,4.7,0);
            glVertex3f(11.7,4.1,0);
            glVertex3f(7.3,3.5,0);

            //fifth
            glVertex3f(12.9,-4.8,0);
            glVertex3f(14.3,-4.4,0);
            glVertex3f(14.3,-0.6,0);
            glVertex3f(12.9,-0.6,0);

            glVertex3f(12.9,4.8,0);
            glVertex3f(14.3,4.4,0);
            glVertex3f(14.3,0.6,0);
            glVertex3f(12.9,0.6,0);

            //sixth
            glVertex3f(12.9,-4.8,0);
            glVertex3f(19.7,-5.7,0);
            glVertex3f(19.7,-5.1,0);
            glVertex3f(14.3,-4.4,0);

            glVertex3f(12.9,4.8,0);
            glVertex3f(19.7,5.7,0);
            glVertex3f(19.7,5.1,0);
            glVertex3f(14.3,4.4,0);

            //seventh
            glVertex3f(21.4,-7.7,0);
            glVertex3f(23,-7.1,0);
            glVertex3f(23,7.1,0);
            glVertex3f(21.4,7.7,0);
        glEnd();
    glPopMatrix();
}

//--Draws a pedestal -------------------------------------------------------
void drawPedestal(bool isShadow)
{
    glPushMatrix();
        glColor3f(0.83, 0.74, 0.55);
        if (isShadow) glColor4f(0.3,0.3,0.3,1);
        glScalef(0.2,0.2,0.2);
        drawMesh(pedestal);
    glPopMatrix();

}

//--Draws a submarine ------------------------------------------------------
void drawSub() {

    glPushMatrix();
        glRotatef(subAngle, 0, 1, 0);
        glTranslatef(0,cos(subAngle)*0.1,0);
        float light1_pos[4] = { light1X, light1Y, light1Z, 1 };
        float light1_dir[3] = { 0, -1, 0 };
        glLightfv(GL_LIGHT1, GL_POSITION, light1_pos);
        glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, light1_dir);
        glColor3f(0.68, 1, 0.18);
        glTranslatef(0,2.2,-2);
        glScalef(0.5,0.5,0.5);
        drawMesh(submarine);
        glTranslatef(0,-0.6,0);
        glRotatef(subAngle*40,1,0,0);
        glColor3f(0.68, 0.7, 0.18);
        glTranslatef(2,0,-1.02);
        glutSolidCylinder(0.2,2,10,10);
        glTranslatef(-2,0,1.02);
        glRotatef(90,1,0,0);
        glTranslatef(2,0,-1.02);
        glutSolidCylinder(0.2,2,10,10);
    glPopMatrix();
}

//--Draws water ------------------------------------------------------------
void drawWater()
{
    glPushMatrix();
        glTranslatef(0,2.5,0);
        glScalef(0.067,0.067,0.067);
        glColor3f(0.14, 0.40,  0.57);			//Floor colour
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL);
        glBegin(GL_QUADS);
        for(int x = -WATER_SIZE/2; x <= WATER_SIZE/2; x++)
        {
            for(int z = -WATER_SIZE/2; z <= WATER_SIZE/2; z++)
            {
                glVertex3f(x, noise.GetNoise((float)(x), (float)(z+noiseOffset))*2, z);
                glVertex3f(x, noise.GetNoise((float)(x), (float)(z+1+noiseOffset))*2, z+1);
                glVertex3f(x+1, noise.GetNoise((float)(x+1), (float)(z+1+noiseOffset))*2, z+1);
                glVertex3f(x+1, noise.GetNoise((float)(x+1), (float)(z+noiseOffset))*2, z);

            }
        }
        glEnd();
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    glPopMatrix();
}

//--Draws a fish tank ------------------------------------------------------
void drawFishTank() {
    glPushMatrix();
        drawWater();
        drawSub();
        glBegin(GL_QUADS);
            //Base
            glNormal3f(0,1,0);
            for(int x = -6/2; x < 6/2; x++)
            {
                for(int z = -6/2; z < 6/2; z++)
                {
                    if ((x+z)%2) {
                        glColor3f(0, 0.19,  0.13);
                    } else {
                        glColor3f(0.11, 0.07,  0.07);
                    }
                    glVertex3f(x*1, 0, z*1);
                    glVertex3f(x*1, 0, (z+1)*1);
                    glVertex3f((x+1)*1, 0, (z+1)*1);
                    glVertex3f((x+1)*1, 0, z*1);
                }
            }
        glEnd();
        glEnable(GL_BLEND);
        glColor4f(0.05,0.52,0.8, 0.5);
        glBegin(GL_QUADS);
            //Walls
            glNormal3f(-1,0,0);
            glVertex3f(-3,0,-3);
            glVertex3f(-3,0,3);
            glVertex3f(-3,3,3);
            glVertex3f(-3,3,-3);

            glNormal3f(0,0,1);
            glVertex3f(-3,0,3);
            glVertex3f(3,0,3);
            glVertex3f(3,3,3);
            glVertex3f(-3,3,3);

            glNormal3f(1,0,0);
            glVertex3f(3,0,3);
            glVertex3f(3,0,-3);
            glVertex3f(3,3,-3);
            glVertex3f(3,3,3);

            glNormal3f(0,0,-1);
            glVertex3f(3,0,-3);
            glVertex3f(-3,0,-3);
            glVertex3f(-3,3,-3);
            glVertex3f(3,3,-3);
        glEnd();
        glDisable(GL_BLEND);
    glPopMatrix();
}

void drawScanimation() {
    glPushMatrix();
        glDisable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, txId[0]);
        glColor3f(0.97,0.74,0.61);
        glBegin(GL_QUADS);
            glNormal3f(1,0,0);
            glTexCoord2f(0, 0);
            glVertex3f(0,0,5);
            glTexCoord2f(1, 0);
            glVertex3f(0,0,-5);
            glTexCoord2f(1, 0.5);
            glVertex3f(0,5,-5);
            glTexCoord2f(0, 0.5);
            glVertex3f(0,5,5);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, txId[1]);
        glTranslatef(0,0,scanAmount);
        glBegin(GL_QUADS);
            glNormal3f(1,0,0);
            glTexCoord2f(0, 0);
            glVertex3f(0.1,0,5);
            glTexCoord2f(1, 0);
            glVertex3f(0.1,0,-5);
            glTexCoord2f(1, 0.5);
            glVertex3f(0.1,5,-5);
            glTexCoord2f(0, 0.5);
            glVertex3f(0.1,5,5);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);
    glPopMatrix();
}

void drawLamp() {
    glPushMatrix();
        glColor3f(0.39,0.26,0.12);
        glRotatef(-90,1,0,0);
        glutSolidCylinder(2,0.5,10,10);
        glutSolidCylinder(0.2,20,10,10);
        glRotatef(90,1,0,0);
        glTranslatef(0,20,0);
        glutSolidDodecahedron();
        glColor3f(0.9,0.8,0);
        glTranslatef(-1,-1,0);
        glDisable(GL_LIGHTING);
        glutSolidSphere(0.5,20,20);
        glEnable(GL_LIGHTING);
    glPopMatrix();
    glPushMatrix();
        glTranslatef(0,ballHeight,0);
        glColor3f(0.2,0.3,0.4);
        glutSolidSphere(1,10,10);
    glPopMatrix();

}

void drawPainting(int img) {
    glPushMatrix();
        glDisable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, txId[img]);
        glBegin(GL_QUADS);
            glColor3f(0.0,0.3,0.2);
            glNormal3f(0,0,1);
            glTexCoord2f(0, 0);
            glVertex3f(-5,-5,0);
            glTexCoord2f(1, 0);
            glVertex3f(5,-5,0);
            glTexCoord2f(1,  0.75);
            glVertex3f(5,5,0);
            glTexCoord2f(0,  0.75);
            glVertex3f(-5,5,0);
        glEnd();
        glEnable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

void normal(int i) {
    float xd1,zd1,xd2,zd2;
    xd1 = i - i - 1;
    zd1 = cos(i) - cos(i-1);
    xd2 = i + 1 - i;
    zd2 = cos(i+1) - cos(i);
    glNormal3f(-(zd1+zd2), 0, (xd1+xd2));
}

void drawCloth() {
    glPushMatrix();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, txId[5]);
    glColor3f(1,1,1);
    glBegin(GL_QUAD_STRIP);
    int n = 20;
    for (int i = 0; i < n; i++) {
        normal(i);
        glTexCoord2f((float)i/(float)(n-1), 0);
        glVertex3f(i,0,cos(i));
        glTexCoord2f((float)i/(float)(n-1), 1);
        glVertex3f(i,10,cos(i));
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

//--Set camera position and direction --------------------------------------
void setCam() {
    glm::vec3 direction;
    direction.x = std::cos(glm::radians(cameraYaw));
    direction.y = 0;
    direction.z = std::sin(glm::radians(cameraYaw));
    cameraFront = glm::normalize(direction);

    glm::vec3 cameraLookPoint = cameraPos + cameraFront;
    gluLookAt(cameraPos.x, cameraPos.y, cameraPos.z,
              cameraLookPoint.x, cameraLookPoint.y, cameraLookPoint.z,
              cameraUp.x, cameraUp.y, cameraUp.z);              //Camera position and orientation
}

//--Display: ---------------------------------------------------------------
//--This is the main display module containing function calls for generating
//--the scene.
void display(void)
{
    float lpos[4] = {60., 70., 60., 1.0};  //light's position

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    setCam();

    glPushMatrix();
        glRotatef(lightAngle, 0, 1, 0);
        glLightfv(GL_LIGHT0,GL_POSITION, lpos);   //Set light position
        glColor3f(1,1,0);
        glDisable(GL_LIGHTING);
        glTranslatef(lpos[0],lpos[1],lpos[2]);
        glutSolidSphere(10,20,20);
        glEnable(GL_LIGHTING);
    glPopMatrix();

    glPushMatrix();
        glTranslatef(0,-0.01,0);
        drawFloor();
        glDisable(GL_LIGHTING);		//Disable lighting when drawing walls.
        drawWalls();
        glEnable(GL_LIGHTING);		//Enable lighting when drawing the models.
    glPopMatrix();

    glPushMatrix();
        float light2_pos[4] = { 8, 34, -40, 1 };
        float light2_dir[3] = { -0.2, -0.7, 0 };
        glLightfv(GL_LIGHT2, GL_POSITION, light2_pos);
        glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, light2_dir);
        float shadowMat[16] = { light2_pos[1], 0, 0, 0, -light2_pos[0], 0, -light2_pos[2],
                                -1, 0, 0, light2_pos[1], 0, 0, 0, 0, light2_pos[1] };
        glTranslatef(8,0,-40);
        drawLamp();
    glPopMatrix();

    glPushMatrix();                 //Transform and draw ames window
        glTranslatef(0, 0,-40);
        drawPedestal(false);
        glTranslatef(0,16, 0);
        glDisable(GL_LIGHTING);		//Disable lighting when drawing floor and window.
        drawAmesWindow(false);      //Draws each side individually
        drawAmesWindow(true);
        glEnable(GL_LIGHTING);		//Enable lighting when drawing the models.
    glPopMatrix();

    glPushMatrix();                 //Draw pedestal shadow
        glDisable(GL_LIGHTING);
        glMultMatrixf(shadowMat);
        glTranslatef(0, 0,-40);
        drawPedestal(true);
        glEnable(GL_LIGHTING);
    glPopMatrix();

    glPushMatrix();                 //Transform and draw paintings
        glTranslatef(20,15,-47.8);
        drawPainting(3);
        glTranslatef(-40,0,0);
        drawPainting(4);
    glPopMatrix();

    glPushMatrix();
        glTranslatef(-5,29,-47);
        glRotatef(180,0,1,0);
        glRotatef(90,0,0,1);
        drawCloth();
    glPopMatrix();

    glPushMatrix();                 //Transform and draw fish tank
        glTranslatef(-40, 0, 0);
        drawPedestal(false);
        glTranslatef(0,13.8, 0);
        drawScanimation();
    glPopMatrix();

    glPushMatrix();                 //Transform and draw fish tank
        glTranslatef(40, 0, 0);
        drawPedestal(false);
        glTranslatef(0,13.81,0);
        drawFishTank();
    glPopMatrix();

    glutSwapBuffers();
}

//--------------------------------------------------------------------------
void initialize(void)
{
    float pink[] = { 1, 0.75, 0.8, 1 };
    float grey[] = { 0.5, 0.5, 0.5, 1};
    float white[] = { 1, 1, 1, 1};
    glClearColor(0.21f, 0.32f, 0.43f, 1.0f);   //Background color
    loadMeshFile("pedestal.off", pedestal);			//Specify mesh file names
    loadMeshFile("submarine.off", submarine);

    loadTexture();
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);

    glEnable(GL_LIGHTING);		//Enable OpenGL states
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glDisable(GL_BLEND);

    glLightfv(GL_LIGHT0, GL_DIFFUSE, grey);

    glEnable(GL_LIGHT1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, white);
    glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, light1Cut);
    glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, light1Exp);

    glEnable(GL_LIGHT2);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, white);
    glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, 20.0);
    glLightf(GL_LIGHT2, GL_SPOT_EXPONENT, 2.0);

    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60, 1, 10.0, 1000.0);   //Camera Frustum
}


//  ------- Main: Initialize glut window and register call backs -----------
int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(600, 600);
    glutInitWindowPosition((glutGet(GLUT_SCREEN_WIDTH))/5,
                           (glutGet(GLUT_SCREEN_HEIGHT))/5);				//Window's position w.r.t top-left corner of screen
    glutCreateWindow("Gallery");
    initialize();
    glutDisplayFunc(display);
    glutSpecialFunc(special);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(100,animationTimer,0);
    glutMainLoop();
    return 0;
}

