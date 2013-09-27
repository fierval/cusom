// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#pragma warning(disable : 4996)

#include <iostream>
#include <tchar.h>

// TODO: reference additional headers your program requires here
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <wchar.h>
#include <conio.h>
#include <time.h>
#include <float.h>

#include <vector>
#include <algorithm>

using namespace std;

#include <mm3dnow.h>
#include <cuda_runtime_api.h>
#include <cuda_runtime.h>

typedef struct _entry {
        float *vec;                //vector
        int size;                  //vector size
        wchar_t fname[_MAX_PATH];  //vector file name
        int cls;                   //vector class
} ENTRY, *PENTRY;

typedef struct _rec {
        vector<PENTRY> entries;         //rec entries
        vector< vector<int> > indices;  //2D array of classes indices
        vector<int> clsnum;             //classes numbers of indeces columns
} REC, *PREC;

/*
   2D type array

    [entry ... vec ...]   (+ size,fname,class type)
    [entry ... vec ...]
    [entry ... vec ...]
    [entry ... vec ...]
     ...
    N = rec.size()

    individual point - rec.entries[y].vec[x]


    indices to y axis of entries vector
     vector<int> x0;  vector<int> x1; vector<int> x2; ... vector<int> xClassesNum

    clsnum[0],clsnum[1], ... clsnum[N]
      [x0][x1][x2][x3] ... [xN]    N different classes
      [x0][x1][x2][x3] ... [xN]
      [x0][x1][x2][x3] ... [xN]
          [x1][x2]
          [x1]
          ...

    example
      rec->clsnum[] =   3, 1, 2    <--  1D vector

                        0  1  8    <--  rec->indices[x].at(y)  2D array
                       10  5  9
                        3  2
                        4  6

                                     */
