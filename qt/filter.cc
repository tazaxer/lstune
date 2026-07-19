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
  // A 4th-order bandpass filter tailored specifically for Bass Guitar (48000Hz fs).
  // 2nd-order Butterworth Highpass @ 10Hz to remove DC rumble
  // 2nd-order Butterworth Lowpass  @ 250Hz to remove high harmonics that confuse strobe/zero-cross tracking
  // Coefficients for x(k), x(k-1), .... x(k-n)
  const fltType cx[] = {2.614106e-04, 0.000000e+00, -5.228212e-04, 0.000000e+00, 2.614106e-04};

  // Coefficients for y(k), y(k-1), .... y(k-n)
  const fltType cy[] = {1.000000e+00, -3.951877e+00, 5.856764e+00, -3.857896e+00, 9.530087e-01};

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

