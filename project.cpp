// Matthew Taylor
//
//
//program: project copied from lab10
//
//program: lab10.cpp
//author:  Gordon Griesel
//date:    Fall 2021
//purpose: Framework for X11 (Xlib, XWindows)
//
//This program implements a wrapper class to handle X11 operations.
//The main function contain an infinite loop.
//Keyboard input is handled.
//Mouse input is handled.
//All drawing is done in the render() function.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <iostream>
#include <fstream>
using namespace std;

#define PI 3.1415926535897932384
#define rnd() ((Flt)rand() / (Flt)RAND_MAX)

typedef double Flt;

class Image {
    public:
        int width, height;
        unsigned char *data;
        Image(const char *fname) {
            ifstream fin(fname);
            if (fin.fail())
            {
                cout << "ERROR - opening image " << fname << endl;
                data = new unsigned char [3];
                width = 1;
                height = 1;
                return;
            }
            char p6[8];
            fin >> p6;
            fin >> width >> height;
            int maxcolor;
            fin >> maxcolor;
            data = new unsigned char [width * height * 3];
            fin.read((char *)data, 1);
            fin.read((char *)data, width*height*3);
            fin.close();
            cout << width << " " << height << endl;
        }
};

class Vec {
    public:
        Flt x,y,z;
        Vec() { x = y = z = 0.0; }
        Vec(Flt a, Flt b, Flt c) {
            x = a;
            y = b;
            z = c;
        }
        Vec(const Vec &v) {
            x = v.x;
            y = v.y;
            z = v.z;
        }
        void copy(const Vec &v) {
            x = v.x;
            y = v.y;
            z = v.z;
        }
        void copy(const unsigned char v[3]) {
            x = v[0] / 255.0;
            y = v[1] / 255.0;
            z = v[2] / 255.0;
        }
        void make(Flt a, Flt b, Flt c) {
            x = a;
            y = b;
            z = c;
        }
        void negate()
        {
            x = -x;
            y = -y;
            z = -z;
        }
        void diff(Vec v0, Vec v1) {
            x = v0.x - v1.x;
            y = v0.y - v1.y;
            z = v0.z - v1.z;
        }
        void sub(Vec v) {
            x = x - v.x;
            y = y - v.y;
            z = z - v.z;
        }
        void add(const Vec &v) {
            x = x + v.x;
            y = y + v.y;
            z = z + v.z;
        }
        void add(const Vec &v0, const Vec &v1) {
            x = v0.x + v1.x;
            y = v0.y + v1.y;
            z = v0.z + v1.z;
        }
        void add(Flt a)
        {
            x = x + a;
            y = y + a;
            z = z + a;
        }
        void mult(Vec v) {
            x *= v.x;
            y *= v.y;
            z *= v.z;
        }
        void scale(Flt a) {
            x *= a;
            y *= a;
            z *= a;
        }
        void normalize() {
            Flt len = sqrt(x*x + y*y + z*z);
            if (len == 0.0)
                return;
            x /= len;
            y /= len;
            z /= len;
        }
        Flt dotProduct(Vec v) {
            return x*v.x + y*v.y + z*v.z;
        }
        Flt len() { return sqrt(x*x + y*y + z*z); }
        void combine(Flt A, Vec a, Flt B, Vec b) {
            x = A * a.x + B * b.x;
            y = A * a.y + B * b.y;
            z = A * a.z + B * b.z;
        }
        void crossProduct(const Vec &v0, const Vec &v1) {
            x = v0.y*v1.z - v1.y*v0.z;
            y = v0.z*v1.x - v1.z*v0.x;
            z = v0.x*v1.y - v1.x*v0.y;
        }
        void clamp(Flt low, Flt hi) {
            if (x < low) x = low;
            if (y < low) y = low;
            if (y < low) z = low;
            if (x > hi) x = hi;
            if (y > hi) y = hi;
            if (z > hi) z = hi;
        }
};


Flt degreesToRadians(Flt angle) {
    return (angle / 360.0) * (3.141592653589793 * 2.0);
}

enum {
    OBJ_TYPE_DISC,
    OBJ_TYPE_RING,
    OBJ_TYPE_SPHERE,
    OBJ_TYPE_TRI
};

class Object {
    public:
        int type;
        Vec pos;
        Flt radius, radius2;
        unsigned char color[3];
        Vec normal;

        //triangle qualities
        Vec tri[3];
        int phong_shaded;
        Vec vertNormal[3];
        Flt u, v, w;

        int specular;
        int perlin2, perlin3;
        int marble2, marble3;
        int stexture;
        int hspecular;
        int self_shadow;


        //clipping
        int nclips;
        Vec cpoint;
        Vec cnormal;
        Flt cradius;
        int inside;

        Object() {
            self_shadow = 0;
            specular = 0;
            perlin2 = 0;
            perlin3 = 0;
            marble2 = 0;
            marble3 = 0;
            stexture = 0;
            hspecular = 0;
            phong_shaded = 0;
            
            nclips = 0;
        }
} obj[ 1000 ];
int nobjects = 0;

class Ray {
    public:
        Vec o;
        Vec d;
};

struct Hit {
    //t     = distance to hit point
    //p     = hit point
    //norm  = normal of surface hit
    //color = color of surface hit
    Flt t;
    Vec p, norm, color;
};

class Camera {
    public:
        Vec pos, at, up;
        Flt ang;
        Camera() {
            pos.make(0.0, 0.0, 1000.0);
            at.make(0.0, 0.0, -1000.0);
            up.make(0.0, 1.0, 0.0);
            ang = 45.0;
        }
} cam;

class Global {
    public:
        //define window resolution
        int xres, yres;
        int type;
        int menu;
        int perspective;
        int hit_idx;
        int background_black;
        //
        Vec light_pos;
        Vec ambient;
        Vec diffuse;
        Global() {
            xres = 640;
            yres = 480;
            type = 0;
            menu = 0;
            perspective = 0;
            hit_idx = -1;
            background_black = 0;
            light_pos.make(100, 100, 100);
            ambient.make(1,1,1);
            diffuse.make(0,0,0);
        }
} g;

class X11_wrapper {
    private:
        Display *dpy;
        Window win;
        GC gc;
    public:
        X11_wrapper();
        ~X11_wrapper();
        bool getPending();
        void getNextEvent(XEvent *e);
        void check_resize(XEvent *e);
        void getWindowAttributes(int *width, int *height);
        XImage *getImage(int width, int height);
        void set_backcolor_3i(int r, int g, int b);
        void set_color_3i(int r, int g, int b);
        void clear_screen();
        void drawString(int x, int y, const char *message);
        void drawPoint(int x, int y);
        void drawLine(int x0, int y0, int x1, int y1);
        void drawRectangle(int x, int y, int w, int h);
        void fillRectangle(int x, int y, int w, int h);
        void check_mouse(XEvent *e);
        int check_keys(XEvent *e);
} x11;

//function prototypes
void physics();
void render();
void getTriangleNormal(Vec tri[3], Vec &norm);

//===============================================
int main()
{
    int done = 0;
    while (!done) {
        //Check the event queue
        while (x11.getPending()) {
            //Handle the events one-by-one
            XEvent e;
            x11.getNextEvent(&e);
            x11.check_resize(&e);
            done = x11.check_keys(&e);
            //render();
        }
        //render();
        usleep(1000);
    }
    return 0;
}
//===============================================

void takeScreenshot(const char *filename, int reset)
{
    //This function will capture your current X11 window,
    //and save it to a PPM P6 image file.
    //File names are generated sequentially.
    static int picnum = 0;
    int x,y;
    int width, height;
    x11.getWindowAttributes(&width, &height);
    if (reset)
        picnum = 0;
    XImage *image = x11.getImage(width, height);
    //
    //If filename argument is empty, generate a sequential filename...
    char ts[256] = "";
    strcpy(ts, filename);
    if (ts[0] == '\0') {
        sprintf(ts,"./lab10%02i.ppm", picnum);
        picnum++;
    }
    FILE *fpo = fopen(ts, "w");
    if (fpo) {
        fprintf(fpo, "P6\n%i %i\n255\n", width, height);
        for (y=0; y<height; y++) {
            for (x=0; x<width; x++) {
                unsigned long pixel = XGetPixel(image, x, y);
                fputc(((pixel & 0x00ff0000)>>16), fpo);
                fputc(((pixel & 0x0000ff00)>> 8), fpo);
                fputc(((pixel & 0x000000ff)    ), fpo);
            }
        }
        fclose(fpo);
    }
    XFree(image);
}

X11_wrapper::X11_wrapper() {
    //constructor
    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "ERROR: could not open display\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    int scr = DefaultScreen(dpy);
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr),
            1, 1, g.xres, g.yres, 0, 0x00ffffff, 0x00400040);
    XStoreName(dpy, win, "CMPS-3480   Press Esc to exit.");
    gc = XCreateGC(dpy, win, 0, NULL);
    XMapWindow(dpy, win);
    XSelectInput(dpy, win, ExposureMask | StructureNotifyMask |
            PointerMotionMask | ButtonPressMask |
            ButtonReleaseMask | KeyPressMask);
}
X11_wrapper::~X11_wrapper() {
    //destructor
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
}
bool X11_wrapper::getPending() {
    return XPending(dpy);
}
void X11_wrapper::getNextEvent(XEvent *e) {
    XNextEvent(dpy, e);
}
void X11_wrapper::check_resize(XEvent *e) {
    //ConfigureNotify is sent when window size changes.
    if (e->type != ConfigureNotify)
        return;
    XConfigureEvent xce = e->xconfigure;
    g.xres = xce.width;
    g.yres = xce.height;
}
void X11_wrapper::getWindowAttributes(int *width, int *height) {
    XWindowAttributes gwa;
    XGetWindowAttributes(dpy, win, &gwa);
    *width = gwa.width;
    *height = gwa.height;
}
XImage *X11_wrapper::getImage(int width, int height) {
    return XGetImage(dpy, win, 0, 0, width, height, AllPlanes, ZPixmap);
}
void X11_wrapper::set_backcolor_3i(int r, int g, int b) {
    unsigned long cref = (r<<16) + (g<<8) + b;
    XSetBackground(dpy, gc, cref);
}
void X11_wrapper::set_color_3i(int r, int g, int b) {
    unsigned long cref = (r<<16) + (g<<8) + b;
    XSetForeground(dpy, gc, cref);
}
void X11_wrapper::clear_screen() {
    XClearWindow(dpy, win);
}
void X11_wrapper::drawString(int x, int y, const char *message) {
    XDrawString(dpy, win, gc, x, y, message, strlen(message));
}
void X11_wrapper::drawPoint(int x, int y) {
    XDrawPoint(dpy, win, gc, x, y);
}
void X11_wrapper::drawLine(int x0, int y0, int x1, int y1) {
    XDrawLine(dpy, win, gc, x0, y0, x1, y1);
}
void X11_wrapper::drawRectangle(int x, int y, int w, int h) {
    //x,y is upper-left corner
    XDrawRectangle(dpy, win, gc, x, y, w, h);
}
void X11_wrapper::fillRectangle(int x, int y, int w, int h) {
    //x,y is upper-left corner
    XFillRectangle(dpy, win, gc, x, y, w, h);
}

int X11_wrapper::check_keys(XEvent *e) {
    if (e->type != KeyPress && e->type != KeyRelease) {
        //not a keyboard event. return.
        return 0;
    }
    int key = XLookupKeysym(&e->xkey, 0);
    //Process only key presses.
    if (e->type != KeyPress)
        return 0;
    switch (key) {
        case XK_p:
            g.perspective ^= 1;
            break;
        case XK_r:
            render();
            break;
        case XK_m:
            g.menu = 1;
            render();
            break;
        case XK_1:
            //floor
            nobjects = 0;
            obj[nobjects].type = OBJ_TYPE_DISC;
            obj[nobjects].pos.make(0.0, 0.0, 0.0);
            obj[nobjects].normal.make(0.0, 1.0, 0.0);
            obj[nobjects].normal.normalize();
            obj[nobjects].radius = 2000.0;
            obj[nobjects].color[0] = 200;
            obj[nobjects].color[1] = 200;
            obj[nobjects].color[2] = 255;
            obj[nobjects].stexture = 0;
            obj[nobjects].hspecular = 1;
            obj[nobjects].specular = 1;
            ++nobjects;

            
            //clipped disc left handle
            obj[nobjects].type = OBJ_TYPE_DISC;
            obj[nobjects].pos.make(-20, 250, 0);
            obj[nobjects].normal.make(0.0, 0.0, 0.2);
            obj[nobjects].normal.normalize();
            obj[nobjects].radius = 60.0;
            obj[nobjects].perlin3 = 1;
            obj[nobjects].marble3 = 1;
            obj[nobjects].specular = 1;
            obj[nobjects].color[0] = 255;
            obj[nobjects].color[1] = 100;
            obj[nobjects].color[2] = 150;
            obj[nobjects].nclips = 0;
            obj[nobjects].cpoint.make(10, 240, 0);
            obj[nobjects].cradius = 80.0;
            obj[nobjects].nclips++;
            nobjects++;
            
            //clipped disc right handle
            obj[nobjects].type = OBJ_TYPE_DISC;
            obj[nobjects].pos.make(20, 250, 0);
            obj[nobjects].normal.make(0.0, 0.0, 0.2);
            obj[nobjects].normal.normalize();
            obj[nobjects].radius = 60.0;
            obj[nobjects].perlin3 = 1;
            obj[nobjects].marble3 = 1;
            obj[nobjects].specular = 1;
            obj[nobjects].color[0] = 255;
            obj[nobjects].color[1] = 100;
            obj[nobjects].color[2] = 150;
            obj[nobjects].nclips = 0;
            obj[nobjects].cpoint.make(-10, 240, 0);
            obj[nobjects].cradius = 80.0;
            obj[nobjects].nclips++;
            nobjects++;



            
            //build perlin vase
            {
                //points on a circle
                int n = 27;
                Flt angle = 0.0;
                Flt inc = (PI * 2.0) / (Flt)n;
                Flt radii[13]  = { 3,3,1,1,3,4.5,5,5,2,1.5,2,3,3 };
                Flt height[13] = { 0,1,2,3,5,7,9,11,12,13.5,15,16,17 };
                Vec pts[40];
                for (int i=0; i<n; i++) {
                    pts[i].x = cos(angle);
                    pts[i].y = 0.0;
                    pts[i].z = sin(angle);
                    angle += inc;
                }
                //13 levels
                //13 levels
                Flt vsize = 20.0;
                for (int i=0; i<12; i++) {
                    for (int j=0; j<n; j++) {
                        int k = (j+1) % n;
                        obj[nobjects].type = OBJ_TYPE_TRI;
                        //across 2 then up
                        obj[nobjects].tri[0].copy(pts[j]);
                        obj[nobjects].tri[1].copy(pts[k]);
                        obj[nobjects].tri[2].copy(pts[k]);
                        //
                        obj[nobjects].tri[0].scale(radii[i]*vsize);
                        obj[nobjects].tri[1].scale(radii[i+1]*vsize);
                        obj[nobjects].tri[2].scale(radii[i]*vsize);
                        obj[nobjects].tri[0].y = height[i]*vsize;
                        obj[nobjects].tri[1].y = height[i+1]*vsize;
                        obj[nobjects].tri[2].y = height[i]*vsize;

                        getTriangleNormal(obj[nobjects].tri, obj[nobjects].normal);

                        obj[nobjects].vertNormal[0].copy(pts[k]);
                        obj[nobjects].vertNormal[1].copy(pts[k]);
                        obj[nobjects].vertNormal[2].copy(pts[j]);
                        obj[nobjects].vertNormal[0].y = 0.0;
                        obj[nobjects].vertNormal[1].y = 0.0;
                        obj[nobjects].vertNormal[2].y = 0.0;
                        obj[nobjects].vertNormal[0].normalize();
                        obj[nobjects].vertNormal[1].normalize();
                        obj[nobjects].vertNormal[2].normalize();
                        

                        obj[nobjects].phong_shaded = 1;
                        obj[nobjects].perlin3 = 1;
                        obj[nobjects].marble3 = 1;
                        obj[nobjects].color[0] = 255;
                        obj[nobjects].color[1] = 110;
                        obj[nobjects].color[2] = 40;
                        obj[nobjects].specular = 1;
                        obj[nobjects].hspecular = 0;
                        obj[nobjects].stexture = 0;
                        ++nobjects;
                        //up diagonal then back
                        obj[nobjects].type = OBJ_TYPE_TRI;
                        obj[nobjects].tri[0].copy(pts[j]);
                        obj[nobjects].tri[1].copy(pts[j]);
                        obj[nobjects].tri[2].copy(pts[k]);
                        //
                        obj[nobjects].tri[0].scale(radii[i]*vsize);
                        obj[nobjects].tri[1].scale(radii[i+1]*vsize);
                        obj[nobjects].tri[2].scale(radii[i+1]*vsize);
                        obj[nobjects].tri[0].y = height[i]*vsize;
                        obj[nobjects].tri[1].y = height[i+1]*vsize;
                        obj[nobjects].tri[2].y = height[i+1]*vsize;

                        getTriangleNormal(obj[nobjects].tri, obj[nobjects].normal);

                        obj[nobjects].vertNormal[0].copy(pts[k]);
                        obj[nobjects].vertNormal[1].copy(pts[j]);
                        obj[nobjects].vertNormal[2].copy(pts[j]);
                        obj[nobjects].vertNormal[0].y = 0.0;
                        obj[nobjects].vertNormal[1].y = 0.0;
                        obj[nobjects].vertNormal[2].y = 0.0;
                        obj[nobjects].vertNormal[0].normalize();
                        obj[nobjects].vertNormal[1].normalize();
                        obj[nobjects].vertNormal[2].normalize();
                        
                        
                        obj[nobjects].phong_shaded = 1;
                        obj[nobjects].perlin3 = 1;
                        obj[nobjects].marble3 = 1;
                        obj[nobjects].color[0] = 255;
                        obj[nobjects].color[1] = 110;
                        obj[nobjects].color[2] = 40;
                        obj[nobjects].specular = 1;
                        obj[nobjects].hspecular = 0;
                        obj[nobjects].stexture = 0;
                        ++nobjects;
                    }
                }
            }

            //stars
            for (int i = 0; i < 50; i++)
            {
                obj[nobjects].type = OBJ_TYPE_DISC;
                obj[nobjects].pos.make(rnd()*2000.0-1000.0, rnd()*1500.0, -2000);
                obj[nobjects].normal.make(0.0, 0.0, 1.0);
                obj[nobjects].normal.normalize();
                obj[nobjects].radius = rnd() * 3.0 + 0.5;
                obj[nobjects].color[0] = rnd() * 50.0 + 200.0;
                obj[nobjects].color[1] = rnd() * 50.0 + 200.0;
                obj[nobjects].color[2] = rnd() * 50.0 + 200.0;
                obj[nobjects].stexture = 0;
                obj[nobjects].specular = 0;
                ++nobjects;
            }

            //
            g.light_pos.make(-450.0, 520.0, 600.0);
            g.ambient.make(.3, .3, .3);
            g.diffuse.make(.6, .6, .6);
            //
            cam.pos.make(-30.0, 100.0, 900.0);
            cam.at.make(0.0, 180.0, 0.0);
            cam.up.make(0.0, 1.0, 0.0);
            cam.ang = 40.0;
            break;
        case XK_equal:
        case XK_minus:
            break;
        case XK_Escape:
            //quitting the program
            return 1;
    }
    return 0;
}

void physics(void)
{
    //no physics in this program yet.
}

void getTriangleNormal(Vec tri[3], Vec &norm)
{
    Vec v0,v1;
    //vecSub(tri[1], tri[0], v0);
    v0.diff(tri[1], tri[0]);
    v1.diff(tri[2], tri[0]);
    norm.crossProduct(v0, v1);
    norm.normalize();
}

bool rayIntersectPlane(Vec n, Vec p0, Vec l0, Vec l, Flt &t) 
{ 
    //source online:
    //https://www.scratchapixel.com/lessons/3d-basic-rendering/
    //minimal-ray-tracer-rendering-simple-shapes/ray-plane-and-
    //ray-disk-intersection
    Flt denom = n.dotProduct(l); 
    if (abs(denom) > 0.000001) {
        Vec p0l0;	   	
        p0l0.diff(p0, l0);
        t = p0l0.dotProduct(n) / denom; 
        return (t >= 0.0); 
    }
    return false; 
}

int rayPlaneIntersect(Vec center, Vec normal, Ray *ray, Hit *hit)
{
    Vec v0;
    v0.diff(ray->o, center);
    Flt dot1 = v0.dotProduct(normal);
    if (dot1 == 0.0)
        return 0;
    Flt dot2 = ray->d.dotProduct(normal);
    if (dot2 == 0.0)
        return 0;
    hit->t = -dot1 / dot2;
    if (hit->t < 0.0)
        return 0;
    hit->p.x = ray->o.x + hit->t * ray->d.x;
    hit->p.y = ray->o.y + hit->t * ray->d.y;
    hit->p.z = ray->o.z + hit->t * ray->d.z;
    return 1;
}

bool pointInTriangle(Vec tri[3], Vec p, Flt *u, Flt *v)
{
    //source: http://blogs.msdn.com/b/rezanour/archive/2011/08/07/
    //
    //This function determines if point p is inside triangle tri.
    //   step 1: 3D half-space tests
    //   step 2: find barycentric coordinates
    //
    Vec cross0, cross1, cross2;
    Vec ba, ca, pa;
    ba.diff(tri[1], tri[0]);
    ca.diff(tri[2], tri[0]);
    pa.diff(p, tri[0]);
    //This is a half-space test
    cross1.crossProduct(ca, pa);
    cross0.crossProduct(ca, ba);
    if (cross0.dotProduct(cross1) < 0.0)
        return false;
    //This is a half-space test
    cross2.crossProduct(ba,pa);
    cross0.crossProduct(ba,ca);
    if (cross0.dotProduct(cross2) < 0.0)
        return false;
    //Point is within 2 half-spaces
    //Get area proportions
    //Area is actually length/2, but we just care about the relationship.
    Flt areaABC = cross0.len();
    Flt areaV = cross1.len();
    Flt areaU = cross2.len();
    *u = areaU / areaABC;
    *v = areaV / areaABC;
    //return true if valid barycentric coordinates.
    return (*u >= 0.0 && *v >= 0.0 && *u + *v <= 1.0);
}

int rayTriangleIntersect(Object *o, Ray *ray, Hit *hit)
{
    //if (rayPlaneIntersect(o->vertNormal[0], o->normal, ray, hit)) {
    if (rayPlaneIntersect(o->tri[0], o->normal, ray, hit)) {
        Flt u,v;
        //if (pointInTriangle(o->vertNormal, hit->p, &u, &v)) {
        if (pointInTriangle(o->tri, hit->p, &u, &v)) {
            Flt w = 1.0 - u - v;
            o->u = u;
            o->v = v;
            o->w = w;
            //if (o->surface == SURF_BARY) {
            //  o->color[0] = u;
            //  o->color[1] = v;
            //  o->color[2] = w;
            //}
            return 1;
        }
    }
    return 0;
}

bool rayDiscIntersect(Object *o, Ray *ray, Hit *hit)
{
    Flt t;
    if (rayIntersectPlane(o->normal, o->pos, ray->o, ray->d, t))
    {
        //Calculate the hit point along the ray.
        hit->t = t;
        //(add this to the ray class)
        //hit.t = t;
        hit->p.x = ray->o.x + ray->d.x * t;
        hit->p.y = ray->o.y + ray->d.y * t;
        hit->p.z = ray->o.z + ray->d.z * t;
        //Measure hit from disc center...
        Vec v;
        v.diff(hit->p, o->pos);
        Flt dist = v.len();
        //
        //Is hit point inside radius?
        if (dist <= o->radius) {
            if (o->cradius > 0.0)
            {
                Vec v;
                v.diff(hit->p, o->cpoint);
                Flt len = v.len();
                if(len <= o->cradius)
                    return 0;
            }
            return true;
        }
    }
    return false;
}

int raySphereIntersect(Object *o, Ray *ray, Hit *hit)
{
    //Log("raySphereIntersect()...\n");
    //Determine if and where a ray intersects a sphere.
    //
    // sphere equation:
    // (p - c) * (p - c) = r * r
    //
    // where:
    // p = point on sphere surface
    // c = center of sphere
    //
    // ray equation:
    // o + d*t
    //
    // where:
    //   o = ray origin
    //   d = ray direction
    //   t = distance along ray, or scalar
    //
    // substitute ray equation into sphere equation
    //
    // (o + t*d - c) * (o + t*d - c) - r * r = 0
    //
    // we want it in this form:
    // a*t*t + b*t + c = 0
    //
    // (o + d*t - c)
    // (o + d*t - c)
    // -------------
    // o*o + o*d*t - o*c + o*d*t + d*t*d*t - d*t*c - o*c + c*d*t + c*c
    // d*t*d*t + o*o + o*d*t - o*c + o*d*t - d*t*c - o*c + c*d*t + c*c
    // d*t*d*t + 2(o*d*t) - 2(c*d*t) + o*o - o*c - o*c + c*c
    // d*t*d*t + 2(o-c)*d*t + o*o - o*c - o*c + c*c
    // d*t*d*t + 2(o-c)*d*t + (o-c)*(o-c)
    //
    // t*t*d*d + t*2*(o-c)*d + (o-c)*(o-c) - r*r
    //
    // a = dx*dx + dy*dy + dz*dz
    // b = 2(ox-cx)*dx + 2(oy-cy)*dy + 2(oz-cz)*dz
    // c = (ox-cx)*(ox-cx) + (oy-cy)*(oy-cy) + (oz-cz)*(oz-cz) - r*r
    //
    // now put it in quadratic form:
    // t = (-b +/- sqrt(b*b - 4ac)) / 2a
    //
    //1. a, b, and c are given to you just above.
    //2. Create variables named a,b,c, and assign the values you see above.
    //3. Look how a,b,c are used in the quadratic equation.
    //4. Make your code solve for t.
    //5. Remember, a quadratic can have 0, 1, or 2 solutions.
    //
    Flt a = ray->d.x*ray->d.x + ray->d.y*ray->d.y + ray->d.z*ray->d.z;
    Flt b = 2.0 * (ray->o.x - o->pos.x) * ray->d.x +
        2.0 * (ray->o.y - o->pos.y) * ray->d.y +
        2.0 * (ray->o.z - o->pos.z) * ray->d.z;
    Flt c = (ray->o.x - o->pos.x) * (ray->o.x - o->pos.x) +
        (ray->o.y - o->pos.y) * (ray->o.y - o->pos.y) +
        (ray->o.z - o->pos.z) * (ray->o.z - o->pos.z) -
        o->radius*o->radius;
    Flt t0,t1;
    //discriminant
    Flt disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return 0;
    disc = sqrt(disc);
    t0 = (-b - disc) / (2.0*a);
    t1 = (-b + disc) / (2.0*a);
    //
    if (t0 > 0.0) {
        hit->p.x = ray->o.x + ray->d.x * t0;
        hit->p.y = ray->o.y + ray->d.y * t0;
        hit->p.z = ray->o.z + ray->d.z * t0;
        //sphereNormal(hit->p, o->center, hit->norm);
        hit->t = t0;
        //Calculate surface normal at hit point of sphere
        o->normal.diff(hit->p, o->pos);
        o->normal.normalize();
        return 1;
    }
    if (t1 > 0.0) {
        hit->p.x = ray->o.x + ray->d.x * t1;
        hit->p.y = ray->o.y + ray->d.y * t1;
        hit->p.z = ray->o.z + ray->d.z * t1;
        hit->t = t1;
        //Calculate surface normal at hit point of sphere
        o->normal.diff(hit->p, o->pos);
        o->normal.normalize();
        return 1;
    }
    return 0;
}

int getShadow(Vec h, Vec v)
{
    Ray ray;
    Hit hit;
    ray.o.make(h.x,h.y,h.z);
    ray.d.make(v.x,v.y,v.z);
    //nudge ray forward just a little...
    ray.o.x += ray.d.x * 0.0000001;
    ray.o.y += ray.d.y * 0.0000001;
    ray.o.z += ray.d.z * 0.0000001;
    for (int k=0; k<nobjects; k++) {
        switch (obj[k].type)
        {
            case OBJ_TYPE_DISC:
                if (rayDiscIntersect(&obj[k], &ray, &hit))
                {
                    return 1;
                }
                break;
            case OBJ_TYPE_SPHERE:
                if (raySphereIntersect(&obj[k], &ray, &hit))
                {
                    //if (k != g.hit_idx)
                    return 1;
                }
                break;
            case OBJ_TYPE_TRI:
                if (rayTriangleIntersect(&obj[k], &ray, &hit))
                {
                    //if (!obj[k].self_shadow)
                    return 1;
                }
                break;

        }
    }
    return 0;
}

void reflect(Vec I, Vec N, Vec &R)
{
    //a source
    //http://paulbourke.net/geometry/reflected/
    //Rr = Ri - 2 N (Ri . N)
    //
    //I = incident vector
    //N = the surface normal
    //R = reflected ray
    Flt dot = I.dotProduct(N);
    Flt len = 2.0 * -dot;
    R.x = len * N.x + I.x;
    R.y = len * N.y + I.y;
    R.z = len * N.z + I.z;
}

Flt my_noise2(Vec vec)
{
    extern float noise2(float vec[2]);
    float v[2] = { (float)vec.x, (float)vec.y };
    return (Flt)noise2(v);
}

Flt my_noise3(Vec vec)
{
    extern float noise3(float vec[3]);
    float v[3] = { (float)vec.x, (float)vec.y, (float)vec.z };
    return (Flt)noise3(v);
}

void trace(Ray *ray, Vec &rgb, Flt weight, int level)
{
    //Trace a ray through the scene of objects.
    Hit hit, closehit;
    closehit.t = 1e9;
    int idx = -1;
    for (int k=0; k<nobjects; k++) {
        switch (obj[k].type) {
            case OBJ_TYPE_SPHERE:
                if (raySphereIntersect(&obj[k], ray, &hit)) {
                    if (hit.t < closehit.t) {
                        closehit.t = hit.t;
                        closehit.p.copy(hit.p);
                        closehit.color.copy(obj[k].color);
                        closehit.norm.copy(obj[k].normal);
                        g.hit_idx = k;
                        idx = k;
                    }
                }
                break;
            case OBJ_TYPE_DISC:
                if (rayDiscIntersect(&obj[k], ray, &hit)) {
                    if (hit.t < closehit.t) {
                        closehit.t = hit.t;
                        closehit.p.copy(hit.p);
                        closehit.color.copy(obj[k].color);
                        closehit.norm.copy(obj[k].normal);
                        g.hit_idx = k;
                        idx = k;
                    }
                }
                break;
            case OBJ_TYPE_TRI:
                if (rayTriangleIntersect(&obj[k], ray, &hit)) {
                    if (hit.t < closehit.t) {
                        closehit.t = hit.t;
                        closehit.p.copy(hit.p);
                        closehit.color.copy(obj[k].color);
                        closehit.norm.copy(obj[k].normal);
                        g.hit_idx = k;
                        idx = k;
                    }
                }
                break;
        }
    }
    if (idx < 0) {
        //ray did not hit an object
        //color the sky
        ray->d.normalize();
        //between 0 and 1
        //Flt blue = ray->d.y * 150;
        Flt y = ray->d.y;
        if (y < 0.0) y = -y;
        rgb.make(0.05, 0.05, 0.01);
        //rgb.make((50.0 - y), (10.0 - y), 0.0);
        //rgb.normalize();
        return;
    }
    //Make the hit object be variable o
    Object *o;
    o = &obj[idx];
    //set color of object
    if (o->perlin3)
    {
        float sz = 120.0f;
        float size = sz;
        Vec col;
        while (size >= 1.0f) {
            Vec vec(closehit.p);
            //Vec vec(sin(closehit.p.x), sin(closehit.p.x), sin(closehit.p.x));
            vec.add(400.0);
            //octave
            vec.scale(1.0/size);
            Flt mag = my_noise3(vec);
            //we have the magnitude
            mag = mag + 0.6;
            mag = mag / 1.2;
            mag = mag * size;
            col.add(mag);
            size = size / 2.0f;
        }
        col.scale(1.0f/sz);
        if (o->marble3) {
            col.x = fabs(sin(col.x * PI));
            col.y = fabs(sin(col.y * PI));
            col.z = fabs(sin(col.z * PI));
        }
        closehit.color.copy(col);
    }
    if (o->perlin2)
    {
        float size = 200.0f;
        Vec col;
        while (size >= 1.0f) {
            Vec vec(closehit.p);
            vec.add(10000.0);
            vec.scale(1.0/size);
            Flt mag = my_noise2(vec);
            //we have the magnitude
            mag = mag + 0.6;
            mag = mag / 1.2;
            mag = mag * size;
            col.add(mag);
            size = size / 2.0f;
        }
        col.scale(1.0f/300.0f);
        if (o->marble2) {
            col.x = fabs(sin(col.x * PI));
            col.y = fabs(sin(col.y * PI));
            col.z = fabs(sin(col.z * PI));
        }
        closehit.color.copy(col);
    }
    /*if (o->marble3 == 1)
    {
        Vec col;
        float vec[3] = {(float)closehit.p.x, (float)closehit.p.y, 
            (float)closehit.p.z};
        vec[0] /= 20.0f;
        vec[1] /= 30.0f;
        vec[2] /= 30.0f;
        //float mag = noise2(vec);
        //
        //turbulize the values
        float size = 200.0f;
        int count = 0;
        extern float noise3(float vec[3]);
        while (size >= 2.0)
        {
            float vec[3] = {(float)(closehit.p.x/20.0 + 
                    closehit.p.y/15.0 + closehit.p.z/15.0)};
            vec[0] += 10000.0f;
            vec[1] += 10000.0f;
            vec[2] += 10000.0f;
            //octave 1
            vec[0] /= size;
            vec[1] /= size;
            vec[2] /= size;
            float mag = noise3(vec);
            //we have a magnitude
            mag = mag + 0.6;
            mag = mag / 1.2;
            mag = mag * size;
            col.x = col.x + mag;
            col.y = col.y + mag;
            col.z = col.z + mag;
            size = size / 2.0f;
            count++;
        }
        //col.scale(1.0f/300.0);

        double val = sin(col.x);

        val = val + 1.0;
        val = val / 2.0;
        col.x = val;
        col.y = val;
        col.z = val;
        closehit.color.copy(col);
    }*/

    if (o->phong_shaded)
    {
        closehit.norm.x = o->vertNormal[0].x*o->u + o->vertNormal[1].x*o->v + o->vertNormal[2].x*o->w;
        closehit.norm.y = o->vertNormal[0].y*o->u + o->vertNormal[1].y*o->v + o->vertNormal[2].y*o->w;
        closehit.norm.z = o->vertNormal[0].z*o->u + o->vertNormal[1].z*o->v + o->vertNormal[2].z*o->w;
        closehit.norm.normalize();
    }

    //
    //collect all the light on the object
    //
    if (o->hspecular == 1)
    {
        //this object is reflective
        Ray tray;
        //recursive call to trace()
        reflect(ray->d, closehit.norm, tray.d);

        //void trace(Ray *ray, Vec &rgb, Flt weight, int level)
        Vec trgb;
        tray.o.copy(closehit.p);
        //nudge the ray
        tray.o.x += tray.d.x * 0.00001;
        tray.o.y += tray.d.y * 0.00001;
        tray.o.z += tray.d.z * 0.00001;
        trace(&tray, trgb, weight * 0.5, level + 1);
        trgb.scale(weight);
        trgb.scale(.3);
        rgb.add(trgb);
    }
    //
    //specular highlight
    if (o->specular == 1)
    {
        //http://en.wikipedia.org/wiki/Specular_highlight
        //Blinn Phong lighting model
        Vec lightDir, halfway;
        lightDir.diff(g.light_pos, closehit.p);
        lightDir.normalize();
        ray->d.normalize();
        halfway.diff(lightDir, ray->d);
        halfway.scale(0.5);
        halfway.normalize();
        Flt dot = halfway.dotProduct(closehit.norm);
        if (dot > 0.0)
        {
            dot = pow(dot, 512.0);
            rgb.x += 1.0 * dot * weight;
            rgb.y += 1.0 * dot * weight;
            rgb.z += 1.0 * dot * weight;
        }
    }
    //vector to the light source
    Vec v;
    v.diff(g.light_pos, closehit.p);
    v.normalize();
    Flt sdot = closehit.norm.dotProduct(v);
    int shadow = 0;
    if (sdot > 0.0)
    {
        //are we in a shadow?
        shadow = getShadow(closehit.p, v);
    }
    //surface normal, dot product
    Flt dot = v.dotProduct(closehit.norm);
    //make it between 0 and 2
    dot += 1.0;
    //make it between 0 and 1
    dot /= 2.0;

    //if (dot < 0.0)
    //    dot = 0.2;
    //mult diffuse light by the dot
    Vec diff(g.diffuse);
    diff.scale(dot);
    Vec strength;
    strength.add(g.ambient);
    if (!shadow)
        strength.add(diff);
    rgb.x += closehit.color.x * strength.x * weight;
    rgb.y += closehit.color.y * strength.y * weight;
    rgb.z += closehit.color.z * strength.z * weight;
    rgb.clamp(0.0, 1.0);
}

void render()
{
    if (g.menu) {
        x11.set_color_3i(255, 255, 0);
        x11.drawString(10,10, "Key presses...");
        x11.drawString(10,20, "1 - Project");
        x11.drawString(10,30, "R - Render");
        g.menu = 0;
        return;
    }
    x11.clear_screen();
    //
    //NEW: setup the camera
    //The camera may be set anywhere looking anywhere.
    Flt fyres = (Flt)g.yres;
    Flt fxres = (Flt)g.xres;
    Flt ty = 1.0 / (fyres - 1.0);
    Flt tx = 1.0 / (fxres - 1.0);
    int px = 1;
    int py = 1;
    Vec up(cam.up);
    up.normalize();
    Flt viewAnglex, aspectRatio;
    Flt frustumheight, frustumwidth;
    Vec rgb, dir, left, out;
    out.diff(cam.at, cam.pos);
    out.normalize();
    aspectRatio = fxres / fyres;
    viewAnglex = degreesToRadians(cam.ang * 0.5);
    frustumwidth = tan(viewAnglex);
    frustumheight = frustumwidth / aspectRatio;
    //frustumwidth is half the distance across screen
    //compute the left and up vectors, for stepping across pixels
    left.crossProduct(out, cam.up);
    left.normalize();
    up.crossProduct(left, out);
    //-------------------------------------------------------------------------
    //
    //shoot a ray through every pixel
    for (int i=g.yres-1; i>=0; i--) {
        for (int j=0; j<g.xres; j++) {
            //make the ray that shoots through a pixel.
            px = j;
            py = i;
            Ray ray;
            ray.o.copy(cam.pos);
            dir.combine(-frustumheight * (2.0 * (Flt)py*ty - 1.0), up,
                    frustumwidth  * (2.0 * (Flt)px*tx - 1.0), left);
            ray.d.add(dir, out);
            ray.d.normalize();
            //
            Vec rgb;
            //
            trace(&ray, rgb, 1.0, 1);
            x11.set_color_3i(rgb.x*255, rgb.y*255, rgb.z*255);
            x11.drawPoint(j, i);
        }
    }
}















