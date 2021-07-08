/* Deface filter
 * main algorithm: (c) Copyright 2020 DeFace authors <https://github.com/ORB-HD/deface>
 * frei0r port by Alfredo Di Napoli and Well-Typed Ltd. <alfredo@well-typed.com>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <frei0r.hpp>
#include <Python.h>

#define RED(n)  ((n>>16) & 0x000000FF)
#define GREEN(n) ((n>>8) & 0x000000FF)
#define BLUE(n)  (n & 0x000000FF)
#define RGB(r,g,b) ((r<<16) + (g <<8) + (b))

#define BOOST(n) { \
  if((*p = *p<<4)<0)>>n; \
  *(p+1) = (*(p+1)<<4)>>n; \
  *(p+2) = (*(p+2)<<4)>>n; }

#define PY_SSIZE_T_CLEAN

/* setup some data to identify the plugin */
const uint32_t black = 0xFF000000;
const uint32_t gray  = 0xFF303030;

typedef struct {
  int16_t w;
  int16_t h;
  uint8_t bpp;
  uint32_t size;
} ScreenGeometry;

#define PIXELAT(x1,y1,s) ((s)+(x1)+ yprecal[y1])// (y1)*(geo->w)))
#define GMERROR(cc1,cc2) ((((RED(cc1)-RED(cc2))*(RED(cc1)-RED(cc2))) +\
  ((GREEN(cc1)-GREEN(cc2)) *(GREEN(cc1)-GREEN(cc2))) + \
                           ((BLUE(cc1)-BLUE(cc2))*(BLUE(cc1)-BLUE(cc2)))))

class Deface: public frei0r::filter {
public:

  f0r_param_double threshold;
  char* moduleName;
  char* get_anonymized_image;
  PyObject *pName, *pModule, *pFunc;
  PyGILState_STATE gstate;

  Deface(unsigned int width, unsigned int height) {

      int c;

      register_param(threshold, "threshold", "The threshold as in the deface algorithm");

      // Initialise the ScreenGeometry which will help us to locate pixels on screen.
      geo = new ScreenGeometry();
      geo->w = width;
      geo->h = height;
      geo->size =  width*height*sizeof(uint32_t);

      if ( geo->size > 0 ) {
          prePixBuffer = (int32_t*)malloc(geo->size);
          conBuffer = (int32_t*)malloc(geo->size);

          yprecal = (int*)malloc(geo->h*2*sizeof(int));
      }
      for(c=0;c<geo->h*2;c++)
          yprecal[c] = geo->w*c;
      for(c=0;c<256;c++) 
          powprecal[c] = c*c;

      Py_Initialize();
      //gstate = PyGILState_Ensure();

      moduleName = "deface.deface";
      get_anonymized_image = "get_anonymized_image";
      /* Error checking of pName left out */

      //pModule = PyImport_ImportModule("deface.deface");

      //if (pModule != NULL) {

      //    pFunc = PyObject_GetAttrString(pModule, get_anonymized_image);
      //    /* pFunc is a new reference */

      //    if (pFunc && PyCallable_Check(pFunc)) {
      //        fprintf(stderr, "found get_anonymized_image\n");
      //    }
      //    else {
      //        if (PyErr_Occurred()) {
      //            PyErr_Print();
      //            fprintf(stderr, "Cannot find function \"%s\"\n", get_anonymized_image);
      //        }
      //    }

      //} else {
      //    PyErr_Print();
      //    fprintf(stderr, "Failed to load \"%s\"\n", moduleName);
      //}

  }

  ~Deface() {
    // Dealloc the ScreenGeometry stuff.
    if ( geo->size > 0 ) {
      free(prePixBuffer);
      free(conBuffer);
      free(yprecal);
    }
    delete geo;

    fprintf(stderr, "Shutting down everything\n");
    //Py_DECREF(pModule);
    //PyGILState_Release(gstate);
    Py_FinalizeEx();
  }

  virtual void update(double time, uint32_t* out, const uint32_t* in) {

    //noop
    int x, y, t;

    for (x=(int)0;x<geo->w;x++) {
        for (y=(int)0;y<geo->h;y++) {
          // Copy original color
          *(out+x+yprecal[y]) = *(in+x+yprecal[y]);
        }
    }

  }


private:
  ScreenGeometry *geo;
  /* buffer where to copy the screen
     a pointer to it is being given back by process() */
  int32_t *prePixBuffer;
  int32_t *conBuffer;
  int *yprecal;
  uint16_t powprecal[256];

};

frei0r::construct<Deface> plugin("Deface",
                                  "Defaceify video, do a form of edge detect",
                                  "Deface authors",
                                  2,0);
