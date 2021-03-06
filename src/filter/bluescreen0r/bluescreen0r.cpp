/* 
 * Bluescreen0r
 * 2009 Hedde Bosman
 *
 * This source code  is free software; you can  redistribute it and/or
 * modify it under the terms of the GNU Public License as published by
 * the Free Software  Foundation; either version 3 of  the License, or
 * (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but  WITHOUT ANY  WARRANTY; without  even the  implied  warranty of
 * MERCHANTABILITY or FITNESS FOR  A PARTICULAR PURPOSE.  Please refer
 * to the GNU Public License for more details.
 *
 * You should  have received  a copy of  the GNU Public  License along
 * with this source code; if  not, write to: Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include "frei0r.hpp"

#include <algorithm>
#include <vector>
#include <utility>
#include <cassert>

#include <iostream>

#include <math.h>


class bluescreen0r : public frei0r::filter
{
private:
	f0r_param_double dist;
	f0r_param_color color;
	
	// returns the distance to 'color', but does not normalize
	inline uint32_t distance(uint32_t pixel) {
		uint32_t d = 0;
		int t;
		t = ((pixel&0x00FF0000) >> 16) - color.b;
		d += t*t;
		t = ((pixel&0x0000FF00) >> 8)  - color.g;
		d += t*t;
		t = ((pixel&0x000000FF) >> 0)  - color.r;
		d += t*t;
		
		return (uint32_t) d; // no sqrtf
	}
public:
	bluescreen0r(unsigned int width, unsigned int height)
	{
		dist = 127;
		
		color.r = 0;
		color.g = 240;
		color.b = 0;
		
		register_param(color,  "Color",    "The color to make transparent (B G R)");
		register_param(dist, "Distance", "Distance to Color (127 is good)");
	}

	virtual void update() {
		const uint32_t* pixel	=in;
			  uint32_t* outpixel= out;
		
		uint32_t distInt = (uint32_t) (dist*dist);
		uint32_t distInt2 = (uint32_t) (dist*dist)/2;
		
		while(pixel != in+size) {
			*outpixel= (*pixel & 0x00FFFFFF); // copy all except alpha
			
			uint32_t d = distance(*pixel); // get distance
			unsigned char a = 255; // default alpha
			if (d < distInt) {
				a = 0;
				if (d > distInt2) {
					a = 256*(d-distInt2)/distInt2;
				}
			}
			*outpixel |= (a<<24);
			
			++outpixel;
			++pixel;
		}
	}
};


frei0r::construct<bluescreen0r> plugin("bluescreen0r", "Color to alpha (blit SRCALPHA)", "Hedde Bosman",0,1);

