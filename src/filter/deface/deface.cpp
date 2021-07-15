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

#include <dlfcn.h>
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

class Deface: public frei0r::filter {
public:

  f0r_param_double threshold;
  // module names
  char *defaceModule, *numpyModule;
  // function names
  char *get_anonymized_image, *numpy_fromstring;
  // modules
  PyObject *pDefaceModule, *pNumpyModule;
  // functions
  PyObject *pGetAnonymizedImage, *pNumpyFromString, *pNumpyAsArray;
  PyGILState_STATE gstate;

  Deface(unsigned int width, unsigned int height) {

      // Thank you Stack Overflow stranger, may bards sing
      // odes to you: https://stackoverflow.com/questions/49784583/numpy-import-fails-on-multiarray-extension-library-when-called-from-embedded-pyt
#ifdef __linux__
      dlopen("libpython3.6m.so.1.0", RTLD_LAZY | RTLD_GLOBAL);
#endif

      global_counter++;

      int c;

      register_param(threshold, "threshold", "The threshold as in the deface algorithm");
      pythonReady = false;

      // Initialise the ScreenGeometry which will help us to locate pixels on screen.
      geo = new ScreenGeometry();
      geo->w = width;
      geo->h = height;
      geo->size =  width*height*sizeof(uint32_t);

      if ( geo->size > 0 ) {
          frameBuffer = (char*)malloc(sizeof(char) * (geo->w * geo->h * 3));

          yprecal = (int*)malloc(geo->h*2*sizeof(int));
      }
      for(c=0;c<geo->h*2;c++)
          yprecal[c] = geo->w*c;

      // We need this gruesome global id to make sure we don't
      // initalise more than one interpreter per process.
      if (global_counter == 2) {
          Py_Initialize();
          gstate = PyGILState_Ensure();
          //fprintf(stdout, "Constructor called \"%d\"\n", global_counter);

          defaceModule   = (char*)"deface.deface";
          numpyModule    = (char*)"numpy";
          get_anonymized_image = (char*)"get_anonymized_image";

          pDefaceModule   = PyImport_ImportModule(defaceModule);
          pNumpyModule    = PyImport_ImportModule(numpyModule);

          if (pDefaceModule != NULL) {
              pGetAnonymizedImage = PyObject_GetAttrString(pDefaceModule, get_anonymized_image);
              /* pGetAnonymizedImage is a new reference */

              if (pGetAnonymizedImage && PyCallable_Check(pGetAnonymizedImage)) {
                  //fprintf(stdout, "Setting pythonReady to true\n");
                  pythonReady = true;
              }
              else {
                  pythonReady = false;
                  if (PyErr_Occurred()) {
                      PyErr_Print();
                      fprintf(stderr, "Cannot find function \"%s\"\n", get_anonymized_image);
                  }
              }

          } else {
              PyErr_Print();
              fprintf(stderr, "Failed to load \"%s\"\n", defaceModule);
          }

          if (pNumpyModule != NULL) {

              numpy_fromstring = (char*)"fromstring";

              pNumpyFromString = PyObject_GetAttrString(pNumpyModule, numpy_fromstring);
              pNumpyAsArray    = PyObject_GetAttrString(pNumpyModule, "asarray");

              if (pNumpyFromString && PyCallable_Check(pNumpyFromString)) {
                  pythonReady = pythonReady && true;
              }
              else {
                  pythonReady = false;
                  if (PyErr_Occurred()) {
                      PyErr_Print();
                      fprintf(stderr, "Cannot find function \"%s\"\n", numpy_fromstring);
                  }
              }

          } else {
              PyErr_Print();
              fprintf(stderr, "Failed to load \"%s\"\n", numpyModule);
          }

      }

  }

  ~Deface() {
    // Dealloc the ScreenGeometry stuff.
    if ( geo->size > 0 ) {
      free(frameBuffer);
      free(yprecal);
    }
    delete geo;

    if (global_counter == 2) {

        fprintf(stdout, "Deface destructor called\n");
        Py_DECREF(pGetAnonymizedImage);
        Py_DECREF(pDefaceModule);
        PyGILState_Release(gstate);
        Py_Finalize();

    }

  }

  virtual void update(double time, uint32_t* out, const uint32_t* in) {

    //noop
    int x, y, t;
    char r,g,b;
    int32_t pixel;
    PyObject *pRawFrameBytes, *pTempBytesIOBuffer, *pNumpyInt8, *pNumpyArray, *pNumpyArray2, *pNumpyReshape;
    PyObject *pNumpyToBytes, *pOutBytes, *pNumpyDelete, *pFrameWithAlpha;
    PyObject *pImgSizeArgs, *pPilImage, *pPilToBytes, *pPilImageToBytes, *pPilImageSave, *pPilImagePng;
    PyObject *pArgs, *pFrame, *pThreshold, *pReplaceWith, *pMaskScale, *pEllipse, *pDrawScores, *pValue;

    if (pythonReady) {

        int myCols, myRows;

        myCols = geo->w;
        myRows = geo->h;

        // Clear the frameBuffer
        memset((void*)frameBuffer, 0x00, sizeof(char) * (myCols * myRows * 3));

        char *pRawBytes, *pMyTest;
        int32_t out_pxl, rgba;

        for (int rows = 0; rows < myRows; rows++) {

            for (int cols = 0; cols < myCols * 3 ; cols+=3) {

              pixel = *(in + (rows * myCols + (cols / 3)));
              r     = RED(pixel);
              g     = GREEN(pixel);
              b     = BLUE(pixel);

              *(frameBuffer+((rows * myCols * 3) + cols))     = r;
              *(frameBuffer+((rows * myCols * 3) + cols) + 1) = g;
              *(frameBuffer+((rows * myCols * 3) + cols) + 2) = b;

            }
        }

        pRawFrameBytes = PyBytes_FromStringAndSize(frameBuffer, 3 * myRows * myCols);

        pNumpyInt8     = PyObject_GetAttrString(pNumpyModule, "uint8");

        pArgs = PyTuple_New(2);
        PyTuple_SetItem(pArgs, 0, pRawFrameBytes);
        PyTuple_SetItem(pArgs, 1, pNumpyInt8);
        pNumpyArray = PyObject_CallObject(pNumpyFromString, pArgs);
        Py_DECREF(pArgs);

        if (pNumpyArray == NULL) {
            fprintf(stderr, "Failed to create numpy array from raw ffmpeg frame\n");
            PyErr_Print();
        }

        // Reshape the array.
        pNumpyReshape = PyObject_GetAttrString(pNumpyArray, "reshape");

        pArgs = PyTuple_New(2);
        pImgSizeArgs = PyTuple_New(3);
        PyTuple_SetItem(pImgSizeArgs, 0, PyLong_FromLong(myRows));  // height first.
        PyTuple_SetItem(pImgSizeArgs, 1, PyLong_FromLong(myCols));
        PyTuple_SetItem(pImgSizeArgs, 2, PyLong_FromLong(3L)); // (r,g,b)

        PyTuple_SetItem(pArgs, 0, pImgSizeArgs);
        pFrame = PyObject_CallObject(pNumpyReshape, pImgSizeArgs);
        Py_DECREF(pImgSizeArgs);
        Py_DECREF(pArgs);

        pArgs = PyTuple_New(6); // get_anonymized_image has 6 arguments

        // def get_anonymized_image(frame,
        //                          threshold: float,
        //                          replacewith: str,
        //                          mask_scale: float,
        //                          ellipse: bool,
        //                          draw_scores: bool,
        //                          ):

        pReplaceWith = PyUnicode_DecodeFSDefault("blur");
        pThreshold   = PyFloat_FromDouble((double)threshold);
        pMaskScale   = PyFloat_FromDouble(1.3);
        pEllipse     = PyBool_FromLong(1L);
        pDrawScores  = PyBool_FromLong(0L);

        PyTuple_SetItem(pArgs, 0, pFrame);
        PyTuple_SetItem(pArgs, 1, pThreshold);
        PyTuple_SetItem(pArgs, 2, pReplaceWith);
        PyTuple_SetItem(pArgs, 3, pMaskScale);
        PyTuple_SetItem(pArgs, 4, pEllipse);
        PyTuple_SetItem(pArgs, 5, pDrawScores);

        pValue = PyObject_CallObject(pGetAnonymizedImage, pArgs);

        Py_DECREF(pArgs);
        if (pValue != NULL) {

            // Copy the raw bytes back into the *out pointer.
            pNumpyToBytes = PyObject_GetAttrString(pFrame, "tobytes");

            pArgs = PyTuple_New(0);
            pOutBytes = PyObject_CallObject(pNumpyToBytes, pArgs);
            Py_DECREF(pArgs);

            char *pRawBytes, *pMyTest;
            pRawBytes = PyBytes_AsString(pOutBytes);

            for (int rows = 0; rows < myRows; rows++) {

                for (int cols = 0; cols < myCols * 3 ; cols+=3) {

                  pixel = *(in + (rows * myCols + (cols / 3)));

                  r = *(pRawBytes+((rows * myCols * 3) + cols));
                  g = *(pRawBytes+((rows * myCols * 3) + cols) + 1);
                  b = *(pRawBytes+((rows * myCols * 3) + cols) + 2);

                  rgba = RGB(r,g,b);

                  *(out + (rows * myCols + (cols / 3))) = rgba;

                }
            }

            Py_DECREF(pOutBytes);
            Py_DECREF(pValue);

        }
        else {

            PyErr_Print();
            fprintf(stderr,"Call failed\n");
            // Drop the frame, write the frame normally.
            for (x=(int)0;x<geo->w;x++) {
                for (y=(int)0;y<geo->h;y++) {
                    // Copy original color
                    *(out+x+yprecal[y]) = *(in+x+yprecal[y]);
                }
            }
        }

    } else {

        for (x=(int)0;x<geo->w;x++) {
            for (y=(int)0;y<geo->h;y++) {
                // Copy original color
                *(out+x+yprecal[y]) = *(in+x+yprecal[y]);
            }
        }

    }

  }


private:
  ScreenGeometry *geo;
  char *frameBuffer;
  int *yprecal;
  bool pythonReady;

  static int global_counter;

};

int Deface::global_counter = 0;

frei0r::construct<Deface> plugin("Deface",
                                  "Defaceify video, do a form of edge detect",
                                  "Deface authors",
                                  2,0);
