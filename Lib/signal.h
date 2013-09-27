
#pragma once



class CSignal
{

        int format;

        HANDLE fp, fpmap;
        LPVOID lpMap;


        bool read11(wchar_t *fname);           //obsolete
        //void read12(wchar_t *fname);         //obsolete
        bool read13(wchar_t *fname);

        void changeext(wchar_t *path, wchar_t *ext);


public:
        CSignal(wchar_t *fname);                  //open existing file
        CSignal(wchar_t *fname, int n, int m);    //create new
        ~CSignal();


        int N, M;                             //NxM size of mapped array
        vector<float *> data;                 //N array of pointers to filemapping
        wchar_t name[_MAX_PATH];              //file name


        void dump(wchar_t *fname);            //dump contents to text file

        void minmax(float *buff, int len, float &min, float &max);
        void nminmax(float *buff, int len, float a, float b);
        void nenergy(float *buff, int len, int L = 2);


};



/*
    reads data from list file

    1.      file1  1
                file2  2
        file3  1
        ....

     files in separate files on disk    1.1 - simple text file
                                    1.2 - ecg like data (header in this file)
                        1.3 - mitbih like format (header in separate file *.hea  [N M])


    AI file format
    2.          file1  1
            x1 x2 x3 ... xn
        file2  2
        x1 x2 x3 ... xn
        file3  1
        x1 x2 x3 ... xn
        ...

     files data in this list file

*/