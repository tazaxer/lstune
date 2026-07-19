/*   Copyright 2010 Craig Eaton
 *
 *   This file is part of LSTune.
 *
 *    LSTune is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) 
 * any later version.
 *
 *   LSTune is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 * 
 *   You should have received a copy of the GNU General Public License along
 * with LSTune. If not, see http://www.gnu.org/licenses/.
 */ 

#include <stdio.h>
#include <math.h>
#include "filter.h"

fltType *dtFilter::get_inbuf()
{
  return &inbuf[order];
}

fltType *dtFilter::get_outbuf()
{
  return &outbuf[order];
}

int dtFilter::filter(int toFilter)
{
  // A coefficients for a band pass filter with a 44100Hz sample frequency
  // This consists of a 1st order high pass filter at 30Hz, and a an 8th order
  // Type 1 Chebychev LP filter with a corner frequency of 2Khz with 4% passband ripple
  // This results in a 9th order system.
  // Coefficients for x(k), x(k-1), .... x(k-n)
  const fltType cx[] = {9.684457e-07, 6.779120e-06, 1.936891e-05, 2.711648e-05, 
			1.355824e-05, -1.355824e-05, -2.711648e-05, -1.936891e-05,
			-6.779120e-06, -9.684457e-07};

  // Coefficients for y(k), y(k-1), .... y(k-n)
  const fltType cy[] = {0, -7.616874e+00, 2.634041e+01, -5.424242e+01,
			7.325881e+01, -6.726091e+01, 4.196424e+01, -1.715111e+01,
			4.166027e+00, -4.581673e-01};

  // Actually do the filtering
  int in_bounds = order + toFilter;
  for (int k = order; k <= in_bounds; k++)
    {
      fltType this_y = 0.0;

      for (int n = 0; n <= order; n++)
        this_y += cx[n]*inbuf[k - n] - cy[n]*outbuf[k-n];
      outbuf[k] = this_y;
    }

  // Put the last n samples into the first n positions.
  for (int k = 0; k <= order; k++)
    {
      inbuf[k] = inbuf[toFilter + k];
      outbuf[k] = outbuf[toFilter + k];
    }

  return 0;
}

dtFilter::~dtFilter()
{
  delete inbuf;
  delete outbuf;
}

dtFilter::dtFilter(int buf_length)
{
  buffer_length = buf_length + order;
  inbuf = new fltType[buffer_length];
  outbuf = new fltType[buffer_length];

  // Just for simplicity, zero out the buffers
  for (int n = 0; n < buffer_length; n++) 
      inbuf[n] = outbuf[n] = 0.0;
}

