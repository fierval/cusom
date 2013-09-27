
#include "stdafx.h"
#include "signal.h"





//////////////////////////////////constructors/destructors////////////////////////////////////////////////////
CSignal::CSignal(wchar_t *fname): format(0), fp(0), fpmap(0), lpMap(0), N(0), M(0)
{
        wcscpy(name, fname);

        if (!read13(fname)) {
                read11(fname);
                //  read12(fname);
        }
}
CSignal::CSignal(wchar_t *fname, int n, int m): format(13), fp(0), fpmap(0), lpMap(0), N(n), M(m)
{
        wcscpy(name, fname);

        fp = CreateFileW(fname, GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (fp == INVALID_HANDLE_VALUE) {
                N = 0;
                M = 0;
                format = 0;
                return;
        }
        fpmap = CreateFileMapping(fp, 0, PAGE_READWRITE, 0, N * M * sizeof(float), 0);
        if (!fpmap) {
                N = 0;
                M = 0;
                format = 0;
                CloseHandle(fp);
                return;
        }
        lpMap = MapViewOfFile(fpmap, FILE_MAP_WRITE, 0, 0, N * M * sizeof(float));
        if (!lpMap) {
                N = 0;
                M = 0;
                format = 0;
                CloseHandle(fpmap);
                CloseHandle(fp);
                return;
        }

        float *pdata = (float *)lpMap;
        data.push_back(pdata);

        memset(lpMap, 0, N*M*sizeof(float));

        wchar_t hdrfile[_MAX_PATH];
        wcscpy(hdrfile, fname);
        changeext(hdrfile, L".hea");
        FILE *fh = _wfopen(hdrfile, L"wt");
        if (fh) {
                fwprintf(fh, L"%d %d", N, M);
                fclose(fh);
        }
}
CSignal::~CSignal()
{
        if (format == 11)
                delete[] data[0];

        //close file mapping
        if (lpMap) {
                UnmapViewOfFile(lpMap);
                if (fpmap) {
                        CloseHandle(fpmap);
                        if (fp && fp != INVALID_HANDLE_VALUE)
                                CloseHandle(fp);
                }
        }
}
//////////////////////////////////constructors/destructors////////////////////////////////////////////////////








////////////////////////////////read data in format 1.3///////////////////////////////////////////////////////
bool CSignal::read13(wchar_t *fname)
{
        wchar_t hdrfile[_MAX_PATH];
        wcscpy(hdrfile, fname);

        changeext(hdrfile, L".hea");
        FILE *fh = _wfopen(hdrfile, L"rt");
        if (fh) {
                if (fwscanf(fh, L"%d %d", &N, &M) == 2) {
                        fclose(fh);

                        fp = CreateFileW(fname, GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
                        if (fp == INVALID_HANDLE_VALUE)
                                return false;

                        LARGE_INTEGER size;
                        if (!GetFileSizeEx(fp, &size))
                                return false;

                        if (__int64(sizeof(float)*N*M) != size.QuadPart)
                                return false;

                        fpmap = CreateFileMapping(fp, 0, PAGE_READWRITE, 0, 0, 0);
                        if (!fpmap) {
                                CloseHandle(fp);
                                return false;
                        }
                        lpMap = MapViewOfFile(fpmap, FILE_MAP_WRITE, 0, 0, 0);
                        if (!lpMap) {
                                CloseHandle(fpmap);
                                CloseHandle(fp);
                                return false;
                        }

                        float *pdata = (float *)lpMap;

                        data.resize(N);
                        for (int n = 0; n < N; n++) {
                                data[n] = pdata;
                                pdata += M;
                        }

                        format = 13;
                        return true;
                } else {
                        N = 0;
                        M = 0;
                        fclose(fh);
                        return false;
                }
        } else
                return false;
}

void CSignal::changeext(wchar_t *path, wchar_t *ext)
{
        for (int i = (int)wcslen(path) - 1; i > 0; i--) {
                if (path[i] == '.') {
                        path[i] = 0;
                        wcscat(path, ext);
                        return;
                }
        }
        wcscat(path, ext);
}
////////////////////////////////read data in format 1.3///////////////////////////////////////////////////////



////////////////////////////////read data in format 1.1///////////////////////////////////////////////////////
bool CSignal::read11(wchar_t *fname)
{
        vector<float> vec;
        float tmp;
        int res;

        FILE *fh;
        if ((fh = _wfopen(fname, L"rt")) == 0)
                return false;

        for (;;) {
                res = fscanf(fh, "%f", &tmp);
                if (res == EOF || res == 0)
                        break;
                else
                        vec.push_back(tmp);
        }

        fclose(fh);

        if (vec.size() < 2)
                return false;

        N = 1;
        M = (int)vec.size();
        float *pdata = new float[M];
        for (int i = 0; i < M; i++)
                pdata[i] = vec[i];

        data.push_back(pdata);

        return true;
}
////////////////////////////////read data in format 1.1///////////////////////////////////////////////////////




////////////////////////////////dump data/////////////////////////////////////////////////////////////////////
void CSignal::dump(wchar_t *fname)
{
        FILE *fh = _wfopen(fname, L"wt");
        if (fh) {
                for (int n = 0; n < N; n++) {
                        float *pdata = data[n];
                        for (int m = 0; m < M; m++)
                                fwprintf(fh, L"%f ", pdata[m]);
                        fwprintf(fh, L"\n");
                }
                fclose(fh);
        }
}
////////////////////////////////dump data/////////////////////////////////////////////////////////////////////







///////////////////////////////normalization//////////////////////////////////////////////////////////////////
void CSignal::minmax(float *buff, int size, float &min, float &max)
{
        max = buff[0];
        min = buff[0];
        for (int i = 1; i < size; i++) {
                if (buff[i] > max)max = buff[i];
                if (buff[i] < min)min = buff[i];
        }
}
void CSignal::nminmax(float *buff, int len, float a, float b)
{
        float min, max;
        minmax(buff, len, min, max);

        for (int i = 0; i < len; i++) {
                if (max - min)
                        buff[i] = (buff[i] - min) * ((b - a) / (max - min)) + a;
                else
                        buff[i] = a;
        }
}
void CSignal::nenergy(float *buff, int len, int L)
{
        float enrg = 0.0f;
        for (int i = 0; i < len; i++)
                enrg += pow(fabs(buff[i]), (float)L);

        enrg = pow(enrg, 1.0f / (float)L);
        if (enrg == 0.0f) enrg = 1.0f;

        for (int i = 0; i < len; i++)
                buff[i] /= enrg;
}
///////////////////////////////normalization//////////////////////////////////////////////////////////////////