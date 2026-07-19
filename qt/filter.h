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

#ifndef FILTER_H
#define FILTER_H


typedef float fltType;

class dtFilter
{
private:
  // Order of system (Changed to 4 for 2nd-order HPF + 2nd-order LPF bass filter)
  static const int order = 4;

  // Length of buffer, including wrap-aroudn padding
  int buffer_length;

  // These are the input + output buffers, including extra 
  // space for the previous n values needed for an nth order filter
  fltType *inbuf;
  fltType *outbuf;

public:
  dtFilter(int buf_length);
  ~dtFilter();
  int filter(int toFilter);

  fltType *get_inbuf();
  fltType *get_outbuf();
};

#endif
