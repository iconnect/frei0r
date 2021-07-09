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
#define RGBA(r,g,b,a) ((r<<24) + (g <<16) + (b<<8) + (a))

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
  char *defaceModule, *imageIoModule, *pilImageModule, *ioModule, *numpyModule;
  // function names
  char *get_anonymized_image, *imread, *pilFromBytes, *BytesIO, *numpy_fromstring;
  // modules
  PyObject *pDefaceModule, *pImageIoModule, *pPilImageModule, *pIoModule, *pNumpyModule;
  // functions
  PyObject *pGetAnonymizedImage, *pImread, *pPilImageFromBytes, *pBytesIO, *pNumpyFromString, *pNumpyAsArray;
  PyGILState_STATE gstate;

  Deface(unsigned int width, unsigned int height) {

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
          prePixBuffer = (int32_t*)malloc(geo->size);
          conBuffer = (int32_t*)malloc(geo->size);

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
          imageIoModule  = (char*)"imageio";
          ioModule       = (char*)"io";
          pilImageModule = (char*)"PIL.Image";
          numpyModule    = (char*)"numpy";
          get_anonymized_image = (char*)"get_anonymized_image";

          pIoModule       = PyImport_ImportModule(ioModule);
          pDefaceModule   = PyImport_ImportModule(defaceModule);
          pImageIoModule  = PyImport_ImportModule(imageIoModule);
          pPilImageModule = PyImport_ImportModule(pilImageModule);
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

          if (pImageIoModule != NULL) {

              imread = (char*)"imread";

              pImread = PyObject_GetAttrString(pImageIoModule, imread);

              if (pImread && PyCallable_Check(pImread)) {
                  pythonReady = pythonReady && true;
              }
              else {
                  pythonReady = false;
                  if (PyErr_Occurred()) {
                      PyErr_Print();
                      fprintf(stderr, "Cannot find function \"%s\"\n", imread);
                  }
              }

          } else {
              PyErr_Print();
              fprintf(stderr, "Failed to load \"%s\"\n", imageIoModule);
          }

          if (pPilImageModule != NULL) {

              pilFromBytes = (char*)"frombytes";

              pPilImageFromBytes = PyObject_GetAttrString(pPilImageModule, pilFromBytes);

              if (pPilImageFromBytes && PyCallable_Check(pPilImageFromBytes)) {
                  pythonReady = pythonReady && true;
              }
              else {
                  pythonReady = false;
                  if (PyErr_Occurred()) {
                      PyErr_Print();
                      fprintf(stderr, "Cannot find function \"%s\"\n", pilFromBytes);
                  }
              }

          } else {
              PyErr_Print();
              fprintf(stderr, "Failed to load \"%s\"\n", pilImageModule);
          }

          if (pIoModule != NULL) {

              BytesIO = (char*)"BytesIO";

              pBytesIO = PyObject_GetAttrString(pIoModule, BytesIO);

              if (pBytesIO && PyCallable_Check(pBytesIO)) {
                  pythonReady = pythonReady && true;
              }
              else {
                  pythonReady = false;
                  if (PyErr_Occurred()) {
                      PyErr_Print();
                      fprintf(stderr, "Cannot find function \"%s\"\n", BytesIO);
                  }
              }

          } else {
              PyErr_Print();
              fprintf(stderr, "Failed to load \"%s\"\n", ioModule);
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
      free(prePixBuffer);
      free(conBuffer);
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
    PyObject *pRawFrameBytes, *pTempBytesIOBuffer, *pNumpyInt8, *pNumpyArray, *pNumpyArray2, *pNumpyReshape;
    PyObject *pNumpyToBytes, *pOutBytes, *pNumpyDelete, *pFrameWithAlpha;
    PyObject *pImgSizeArgs, *pPilImage, *pPilToBytes, *pPilImageToBytes, *pPilImageSave, *pPilImagePng;
    PyObject *pArgs, *pFrame, *pThreshold, *pReplaceWith, *pMaskScale, *pEllipse, *pDrawScores, *pValue;

    if (pythonReady) {

        // Copy the frame into conBuffer
        memcpy((void*)conBuffer, (void*)in, geo->size);

        PyObject *test;
        char *pRawBytes, *pMyTest, *slab;

        slab = (char*)malloc(sizeof(char) * (geo->w * geo->h * 3));
        for (x=(int)0;x<geo->w;x++) {
            for (y=(int)0;y<geo->h;y++) {

              char r,g,b;
              int32_t pixel;

              pixel = *(in+x+yprecal[y]);
              r = RED(pixel);
              g = GREEN(pixel);
              b = BLUE(pixel);

              *(slab+x+yprecal[y]) = r;
              *(slab+x+yprecal[y] + 1) = g;
              *(slab+x+yprecal[y] + 2) = b;

            }
        }

        pRawFrameBytes = PyBytes_FromStringAndSize(slab, 3 * (geo->w * geo->h));



        //PyObject_Print(PyLong_FromLong(geo->size), stderr, 0);

        //pRawFrameBytes = PyBytes_FromStringAndSize((char*)conBuffer, geo->size);
        pNumpyInt8     = PyObject_GetAttrString(pNumpyModule, "uint8");

        pArgs = PyTuple_New(2);
        PyTuple_SetItem(pArgs, 0, pRawFrameBytes);
        PyTuple_SetItem(pArgs, 1, pNumpyInt8);
        pNumpyArray = PyObject_CallObject(pNumpyFromString, pArgs);
        Py_DECREF(pArgs);

        if (pNumpyAsArray == NULL) {
            fprintf(stderr, "Failed to create numpy array from raw ffmpeg frame\n");
            PyErr_Print();
        }

        //PyObject_Print(pNumpyArray, stderr, 0);

        // Reshape the array.
        pNumpyReshape = PyObject_GetAttrString(pNumpyArray, "reshape");
        //PyObject_Print(pNumpyReshape, stderr, 0);

        pArgs = PyTuple_New(2);
        pImgSizeArgs = PyTuple_New(3);
        PyTuple_SetItem(pImgSizeArgs, 0, PyLong_FromLong(geo->h));  // height first.
        PyTuple_SetItem(pImgSizeArgs, 1, PyLong_FromLong(geo->w));
        PyTuple_SetItem(pImgSizeArgs, 2, PyLong_FromLong(3L)); // (r,g,b)

        PyTuple_SetItem(pArgs, 0, pImgSizeArgs);
        pFrame = PyObject_CallObject(pNumpyReshape, pImgSizeArgs);
        Py_DECREF(pImgSizeArgs);
        Py_DECREF(pArgs);

        //PyObject_Print(pFrameWithAlpha, stderr, 0);

        // Drop the alpha column
        //pNumpyDelete = PyObject_GetAttrString(pNumpyModule, "delete");
        //pArgs = PyTuple_New(3);
        //PyTuple_SetItem(pArgs, 0, pFrameWithAlpha);
        //PyTuple_SetItem(pArgs, 1, PyLong_FromLong(3L));
        //PyTuple_SetItem(pArgs, 2, PyLong_FromLong(2L));
        //pFrame = PyObject_CallObject(pNumpyDelete, pArgs);
        //Py_DECREF(pArgs);

        ////PyObject_Print(pFrame, stderr, 0);

        //if (pFrame == NULL) {
        //    fprintf(stderr, "Failed to reshape numpy array\n");
        //    PyErr_Print();
        //}

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

            PyObject *test;
            char *pRawBytes, *pMyTest, *slab;
            pRawBytes = PyBytes_AsString(pOutBytes);

            // Test here
            slab = (char*)malloc(sizeof(char) * (geo->w * geo->h * 3));
            for (x=(int)0;x<geo->w;x++) {
                for (y=(int)0;y<geo->h;y++) {

                  char r,g,b;
                  int32_t pixel;

                  pixel = *(in+x+yprecal[y]);
                  r = RED(pixel);
                  g = GREEN(pixel);
                  b = BLUE(pixel);

                  *(slab+x+yprecal[y]) = r;
                  *(slab+x+yprecal[y] + 1) = g;
                  *(slab+x+yprecal[y] + 2) = b;

                }
            }

            test = PyBytes_FromStringAndSize(slab, 3 * (geo->w * geo->h));
            pMyTest   = PyBytes_AsString(test);

            /* What we know
               conBuffer has the right data, if we do:

               *(out+x+yprecal[y]) = *(conBuffer+x+yprecal[y]);

               it works. This already breaks everything:

               test = PyBytes_FromStringAndSize((char*)conBuffer, geo->size);

               likely due to the (char*) conversion.

            */

            // Store
            for (x=(int)0;x<geo->w;x++) {
                for (y=(int)0;y<geo->h;y++) {
                  //*(out+x+yprecal[y]) = *(pMyTest+x+yprecal[y]);

                  char r,g,b;
                  int32_t rgba;

                  //r = *(slab + (x * geo->w) + y);
                  //g = *(slab + (x * geo->w) + y + 1);
                  //b = *(slab + (x * geo->w) + y + 2);

                  r = *(pRawBytes+x+yprecal[y]);
                  g = *(pRawBytes+x+yprecal[y] + 1);
                  b = *(pRawBytes+x+yprecal[y] + 2);

                  rgba = RGBA(r,g,b, 0xFF);

                  *(out+x+yprecal[y]) = rgba;


                }
            }
            Py_DECREF(pOutBytes);
            Py_DECREF(pValue);

        }
        else {
            PyErr_Print();
            fprintf(stderr,"Call failed\n");
        }


    } else {

        for (x=(int)0;x<geo->w;x++) {
            for (y=(int)0;y<geo->h;y++) {
                // Copy original color
                //*(out+x+yprecal[y]) = *(in+x+yprecal[y]);
            }
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
  bool pythonReady;

  static int global_counter;

};

int Deface::global_counter = 0;

frei0r::construct<Deface> plugin("Deface",
                                  "Defaceify video, do a form of edge detect",
                                  "Deface authors",
                                  2,0);
