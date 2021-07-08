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
  PyObject *pName, *pModule;

  Deface(unsigned int width, unsigned int height) {

    Py_Initialize();

    register_param(threshold, "threshold", "The threshold as in the deface algorithm");

    pName = PyUnicode_DecodeFSDefault("deface");
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

  }

  ~Deface() {
    Py_DECREF(pModule);
    Py_FinalizeEx();
  }

  virtual void update(double time, uint32_t* out, const uint32_t* in) {
  }
};

frei0r::construct<Deface> plugin("Deface",
                                  "Defaceify video, do a form of edge detect",
                                  "Deface authors",
                                  2,0);
