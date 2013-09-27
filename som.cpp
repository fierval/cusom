// som.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "lib\signal.h"
#include "libsom\libsom.h"
#include "libsom\node.h"
#include "common.h"

typedef struct _acur {
        float se;
        float sp;
        float pp;
        float np;
        float ac;
} ACUR, *PACUR;


vector<CSignal *> signals;      //file mapping classes of signals
int vector_length = 0;          //size of the first vector read from read_class() trn,vld or tst set

enum SOM::Normalization normalization = SOM::NONE;        
enum SOM::TrainMode train_mode = SOM::SLOW;
enum Node::DistanceMetric distance_metric = Node::EUCL;

SOM *som;


void read_class(FILE *fp, PREC rec, int c = 0);   //closes fp handle
int read_line(FILE *f, wchar_t *buff, int *c = 0);
void get_file_name(wchar_t *path, wchar_t *name);
int parse_path(wchar_t *path, wchar_t *dir, wchar_t *name);
void msec_to_time(int msec, int& h, int& m, int& s, int& ms);


void train(int argc, wchar_t* argv[]);
void validate2(PREC rec, PACUR pacur);         //2 class version validate get results to pacur
void set_validation(PREC vld, PREC trn, float p);      //get random % from trn -> vld
void dump_sets(PREC trn, PREC vld, PREC tst);   //dump trn,vld,tst sets
void test(int argc, wchar_t* argv[]);

void save_weights(int argc, wchar_t* argv[]);

/*
  //training mode
   1 t          //train
   2 net.som      //network conf file
   3 class.txt   //files/vecs for class
   4 epochs     //epochs num
    5 [test]     //test data. random 50% from train set if file is empty
   6 [slow]     //fast=1/[slow=0] training
   7 [dist]     //distance [0-euclidian],1-sosd,2-taxicab,3-angle
   8 [norm]     //normilize input data [0-none],1-minmax,2-zscore,3-softmax,4-energy

  //test mode
   1 r          //run
   2 net.som     //network file
   3 class.txt  //files/vecs to test
                                                            */


int _tmain(int argc, wchar_t* argv[])
{
        if (argc == 1) {
                wprintf(L" 1 t-train\n 2 net.som\n 3 class.txt\n 4 epochs\n  5 [test set]\n 6 [slow] [slow=0]fast=1 training\n 7 [dist] distance [0-euclidian],1-sosd,2-taxicab,3-angle\n 8 [norm] normilize input data [0-none],1-minmax,2-zscore,3-softmax,4-energy \n\n");
                wprintf(L" 1 r-run\n 2 net.som\n 3 class.txt");
        } else if (!wcscmp(argv[1], L"t") && argc >= 1 + 4)
                train(argc, argv);
        else if (!wcscmp(argv[1], L"r") && argc >= 1 + 3)
                test(argc, argv);
        else if (!wcscmp(argv[1], L"w") && argc >= 1 + 2)
                save_weights(argc, argv);
        else
                wprintf(L" bad params.\n");

        return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
void train(int argc, wchar_t* argv[])
{

        REC trnrec;       //train records
        REC tstrec;       //test records


///load data//////////////////////////////////////
        wprintf(L"loading data...\n");
        FILE *cls = _wfopen(argv[3], L"rt");

        if (!cls) {
                wprintf(L"failed to open files\n");
                exit(1);
        } else
                read_class(cls, &trnrec);

        if (!trnrec.entries.size()) {
                wprintf(L" no files loaded.\n");
                exit(1);
        } else {
                wprintf(L" training set: %d files loaded,  size: %d samples\n", trnrec.entries.size(), trnrec.entries[0]->size);
                wprintf(L" classes number: %d\n", trnrec.indices.size());
        }


        int offs = 0;  //offset from 5 param
        //check for tstrec///////////////////////////////////
        if (argc >= 1 + 5) {
                if (wcslen(argv[5]) > 1) { //5-test.txt   6,7,8 - slow/fast,dist,norm
                        offs = 1;
                        FILE *test = _wfopen(argv[5], L"rt");
                        if (test) {
                                read_class(test, &tstrec);
                                if (tstrec.entries.size())
                                        wprintf(L" test size: %d files\n", tstrec.entries.size());
                                else
                                        set_validation(&tstrec, &trnrec, 50.0f);

                                dump_sets(&trnrec, 0, &tstrec);
                        } else {
                                wprintf(L" failed to open %s\n", argv[5]);
                                exit(1);
                        }
                }
        }


        //init som///////////////////////////////////////////
        som = new SOM(argv[2]);
        if (som->status() < 0) {
                wprintf(L" failed to load SOM: %d", som->status());
                exit(1);
        }
        if (som->get_weights_per_node() > vector_length) {
                wprintf(L" weights per node: %d is not equal to data dimension: %d", som->get_weights_per_node(), vector_length);
                exit(1);
        }
        if (som->get_weights_per_node() < vector_length) {
                wprintf(L" weights per node: %d is less than data dimension: %d", som->get_weights_per_node(), vector_length);
        }



        //slow/fast training
        if (argc >= 1 + 5 + offs) {
                int speed = _wtoi(argv[5 + offs]);
                if (SOM::SLOW == speed)
                        train_mode = SOM::SLOW;
                else if (SOM::FAST == speed)
                        train_mode = SOM::FAST;
        }        
        som->set_train_mode(train_mode);

        //distance metric
        if (argc >= 1 + 6 + offs) {
                int dist = _wtoi(argv[6 + offs]);
                if (Node::EUCL == dist)
                        distance_metric = Node::EUCL;
                else if(Node::SOSD == dist)
                        distance_metric = Node::SOSD;
                else if(Node::TXCB == dist)
                        distance_metric = Node::TXCB;
                else if(Node::ANGL == dist)
                        distance_metric = Node::ANGL;
        }
        som->set_distance_metric(distance_metric);

        //normalize train/test data
        if (argc >= 1 + 7 + offs) {
                int norm = _wtoi(argv[7 + offs]);
                if (SOM::NONE == norm)
                        normalization = SOM::NONE;
                else if(SOM::MNMX == norm)
                        normalization = SOM::MNMX;
                else if(SOM::ZSCR == norm)
                        normalization = SOM::ZSCR;
                else if(SOM::SIGM == norm)
                        normalization = SOM::SIGM;
                else if(SOM::ENRG == norm)
                        normalization = SOM::ENRG;
        }
        som->compute_normalization(&trnrec, normalization);	         

        wprintf(L"normalization: %s\n", SOM::g_normalization[normalization]);
        wprintf(L"distance metric: %s\n", SOM::g_distance[distance_metric]);
        wprintf(L"train mode: %s\n", SOM::g_trainmode[train_mode]);
        

        //array of vectors for training
        int trn_index = 0;
        vector<float *> train_vectors;         

        train_vectors.resize(trnrec.entries.size() - tstrec.entries.size());
        for (int i = 0; i < (int)trnrec.entries.size(); i++) {
                if (trnrec.entries[i] == 0)
                        continue;
                train_vectors[trn_index++] = trnrec.entries[i]->vec;
        }

        
        int len = trnrec.entries[0]->size;
        float * trnvecs2d = (float *) malloc(sizeof(float) * (int) train_vectors.size());
        for (int i = 0; i < (int) train_vectors.size(); i++)
        {
            memcpy((void *)(trnvecs2d + i * len), train_vectors.at(i), sizeof(float) * len);
        }
        
        // allocate and copy to cuda
        float * cudaTrainVectors;
        cudaMalloc(&cudaTrainVectors, sizeof(float) * len * train_vectors.size());
        cudaMemcpy(cudaTrainVectors, trnvecs2d, sizeof(float) * len * train_vectors.size(), cudaMemcpyHostToDevice);

        free(trnvecs2d);

        //get radius of half the SOM size////////////////////////////////////
        float R, R0 = som->R0();  
        float nrule, nrule0 = 0.9f;

        int epochs = _wtoi(argv[4]);
        float N = (float)epochs;
        int x = 0;  //0...N-1
        //training////////////////////////////////////////////////////////////////////////////////////
        while (epochs) {
                //exponential shrinking
                R = R0 * exp(-10.0f * (x * x) / (N * N));          //radius shrinks over time
                nrule = nrule0 * exp(-10.0f * (x * x) / (N * N));  //learning rule shrinks over time
                x++;

                som->train(&train_vectors, R, nrule);
                wprintf(L"  epoch: %d    R: %.2f nrule: %g \n", (epochs-- - 1), R, nrule);

                if (kbhit() && _getwch() == 'q') //quit program ?
                        epochs = 0;
        }
        //////////////////////////////////////////////////////////////////////////////////////////////



        wchar_t som_map[_MAX_PATH] = L"";
        get_file_name(argv[2], som_map);
        wcscat(som_map, L"_map");
        som->save_2D_distance_map(som_map);

        ////////////////saving SOMs///////////////////////////////////////////////////////////////////////
        wchar_t som1_name[_MAX_PATH] = L"";        
        wchar_t som2_name[_MAX_PATH] = L"";        
        bool are_0class_vectors = false;
        for (int c = 0; c < (int)trnrec.clsnum.size(); c++) {
                if (trnrec.clsnum[c] == 0) {
                        are_0class_vectors = true;
                        break;
                }
        }

        if (are_0class_vectors == false) { //classes must be with no 0 m_class                
                get_file_name(argv[2], som1_name);
                wcscat(som1_name, L"_1.som");

                //voting scheme: best node of vector///////////////////////////////////////////
                som->vote_nodes_from(&trnrec);
                som->save(som1_name);

                float prsc = 0.0f;
                for (int i = 0; i < som->get_nodes_number(); i++)
                        prsc += som->get_node(i)->get_precision();
                wprintf(L"\n SOM %s precision: %g\n", som1_name, prsc / (float)som->get_nodes_number());                
        }
                
        get_file_name(argv[2], som2_name);
        wcscat(som2_name, L"_2.som");
        if (are_0class_vectors == false)
                //direct scheme best vector for a node/////////////////////////////////////////
                som->assign_nodes_to(&trnrec);
        som->save(som2_name);
        ////////////////saving SOMs///////////////////////////////////////////////////////////////////////



        //testing network on trn,tst sets////////////////////////////////////////////////
        if (trnrec.clsnum.size() == 2) { //if 2 classes output se,sp, ... values   -   validate2()
                ACUR pacur;

                if (wcslen(som1_name) > 0) {
                        som = new SOM(som1_name);
                        if (som->status() == 0) {    //classification results for som_1 network
                                wprintf(L"\nclassification results: %s\n", som1_name);
                                validate2(&trnrec, &pacur);
                                wprintf(L" \n train set: %d %d\n   sensitivity: %.2f\n   specificity: %.2f\n   +predictive: %.2f\n   -predictive: %.2f\n      accuracy: %.2f\n", trnrec.indices[0].size(), trnrec.indices[1].size(), pacur.se, pacur.sp, pacur.pp, pacur.np, pacur.ac);
                                if (tstrec.entries.size()) {
                                        validate2(&tstrec, &pacur);
                                        wprintf(L" \n test set: %d %d\n   sensitivity: %.2f\n   specificity: %.2f\n   +predictive: %.2f\n   -predictive: %.2f\n      accuracy: %.2f\n", tstrec.indices[0].size(), tstrec.indices[1].size(), pacur.se, pacur.sp, pacur.pp, pacur.np, pacur.ac);
                                }
                        } else
                                wprintf(L"failed to load %s for classification. status: %d\n", som1_name, som->status());
                }

                if (wcslen(som2_name) > 0) {
                        som = new SOM(som2_name);
                        if (som->status() == 0) {  //classification results for som_2 network
                                wprintf(L"\nclassification results: %s\n", som2_name);
                                validate2(&trnrec, &pacur);
                                wprintf(L" \n train set: %d %d\n   sensitivity: %.2f\n   specificity: %.2f\n   +predictive: %.2f\n   -predictive: %.2f\n      accuracy: %.2f\n", trnrec.indices[0].size(), trnrec.indices[1].size(), pacur.se, pacur.sp, pacur.pp, pacur.np, pacur.ac);
                                if (tstrec.entries.size()) {
                                        validate2(&tstrec, &pacur);
                                        wprintf(L" \n test set: %d %d\n   sensitivity: %.2f\n   specificity: %.2f\n   +predictive: %.2f\n   -predictive: %.2f\n      accuracy: %.2f\n", tstrec.indices[0].size(), tstrec.indices[1].size(), pacur.se, pacur.sp, pacur.pp, pacur.np, pacur.ac);
                                }
                        } else
                                wprintf(L"failed to load %s for classification. status: %d\n", som2_name, som->status());
                }
        }
        //else need to get Se,Pp for every class   -   validate()

}
////////////////////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////////////////////
// random p % from every class
void set_validation(PREC vld, PREC trn, float p)
{
        int vld_size = 0;
        int class_size = 0;

        vector<int> c;
        wprintf(L" validaton size:");
        for (int i = 0; i < (int)trn->indices.size(); i++) {
                class_size = int((p / 100.0f) * (float)trn->indices[i].size());
                if (class_size < 1) {
                        wprintf(L" validaton is not set, one of the vld class of 0 lenght\n");
                        return;
                }
                vld_size += class_size;
                c.push_back(class_size);
                wprintf(L" %d", class_size);                
        }
        wprintf(L"\n");


        vld->entries.resize(vld_size);
        for (int i = 0; i < (int)trn->indices.size(); i++) {
                vld->clsnum.push_back(trn->clsnum[i]);
                random_shuffle(trn->indices[i].begin(), trn->indices[i].end());
        }

        class_size = 0;        
        //class 0,1,2 ... c.size() ////////////////////////////////////////////
        for (int j = 0; j < (int)c.size(); j++) {
                vector<int> indices;
                indices.resize(c[j]);
                vld->indices.push_back(indices);
                //get random % from trn set
                for (int i = 0; i < c[j]; i++) {
                        int ind = trn->indices[j].at(i);

                        vld->indices[j].at(i) = i + class_size;
                        vld->entries[i+class_size] = trn->entries[ ind ];
                        trn->entries[ ind ] = 0;
                }
                trn->indices[j].erase(trn->indices[j].begin(), trn->indices[j].begin() + c[j]);
                class_size += c[j];
        }        
}

void dump_sets(PREC trn, PREC vld, PREC tst)
{
        wchar_t name[_MAX_PATH] = L"";
        wchar_t dir[_MAX_PATH] = L"";

        FILE* fp = _wfopen(L"dbgsets.txt", L"wt");

        if (trn != 0) {
                size_t s = 0;
                for (size_t i = 0; i < trn->entries.size(); i++) {
                        if (trn->entries[i] != 0) s++;
                }

                fwprintf(fp, L"TRAINING SET: %d\n", s);
                if (trn->entries.size() < 1000) {
                        for (size_t i = 0; i < trn->entries.size(); i++) {
                                if (trn->entries[i] != 0)  //in train set might be 0 entries after setvld()
                                        fwprintf(fp, L"%s  %d\n", trn->entries[i]->fname, trn->entries[i]->cls);
                        }
                }
        }

        if (vld != 0) {
                fwprintf(fp, L"\n\nVALIDATION SET: %d\n", vld->entries.size());
                if (vld->entries.size() < 1000) {
                        for (size_t i = 0; i < vld->entries.size(); i++)
                                fwprintf(fp, L"%s  %d\n", vld->entries[i]->fname, vld->entries[i]->cls);
                }
        }

        if (tst != 0) {
                fwprintf(fp, L"\n\nTEST SET: %d\n", tst->entries.size());
                if (tst->entries.size() < 1000) {
                        for (size_t i = 0; i < tst->entries.size(); i++)
                                fwprintf(fp, L"%s  %d\n", tst->entries[i]->fname, tst->entries[i]->cls);
                }
        }

        fclose(fp);
}

////////////// 2 class version validate //////////////////////////////////////////////////////////////////
void validate2(PREC rec, PACUR pacur)
{
        float se = 0.0f, sp = 0.0f, pp = 0.0f, np = 0.0f, ac = 0.0f;

        int TP = 0, FN = 0, TN = 0, FP = 0;
        ////////////////////TESTING////////////////////////////////////////////////////////////////
        for (int i = 0; i < (int)rec->entries.size(); i++) {                
                if (rec->entries[i] == 0)
                        continue;
                const Node *bmu = som->classify(rec->entries[i]->vec);
                int cls = bmu->get_class();

                //check positives/negatives///////////////////////////////////
                if (rec->entries[i]->cls == cls) {
                        if (cls == 1)
                                TP++;
                        else if (cls == 2)
                                TN++;
                } else { /////error//////////
                        if (cls == 2 && rec->entries[i]->cls == 1)  //ill defined as healthy
                                FN++;
                        else if (cls == 1 && rec->entries[i]->cls == 2)  //healthy defined as ill
                                FP++;
                }
        }
        ///////////////////////////////////////////////////////////////////////////////////////////

        if (TP)
                se = float(TP) / float(TP + FN);
        if (TN)
                sp = float(TN) / float(TN + FP);
        if (TP)
                pp = float(TP) / float(TP + FP);
        if (TN)
                np = float(TN) / float(TN + FN);
        if (TP || FP || FN || TN)
                ac = float(TP + TN) / float(TP + FN + TN + FP);

        pacur->se = 100.0f * se;
        pacur->sp = 100.0f * sp;
        pacur->pp = 100.0f * pp;
        pacur->np = 100.0f * np;
        pacur->ac = 100.0f * ac;
}
////////////////////////////////////////////////////////////////////////////////////////////////











////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////testing/////////////////////////////////////////////////////////////
void test(int argc, wchar_t* argv[])
{
        //Se,Sp, only for 2 classes
        //otherwise Ac

        REC tstrec;

        //load data
        wprintf(L"loading data...\n");
        FILE *cls = _wfopen(argv[3], L"rt");

        if (!cls) {
                wprintf(L"failed to open files\n");
                exit(1);
        } else
                read_class(cls, &tstrec);

        if (!tstrec.entries.size()) {
                wprintf(L" no files loaded.\n");
                exit(1);
        } else {
                wprintf(L" training set: %d files loaded,  size: %d samples\n", tstrec.entries.size(), tstrec.entries[0]->size);
                wprintf(L" classes number: %d\n", tstrec.indices.size());
        }


        //init som
        som = new SOM(argv[2]);
        if (som->status() != 0) {
                wprintf(L" failed to load SOM: %d", som->status());
                exit(1);
        }
        wprintf(L"%s\n", argv[2]);
        

        wchar_t name[_MAX_PATH] = L"";
        wchar_t dir[_MAX_PATH] = L"";

        int pos = 0, TP = 0, TN = 0, FP = 0, FN = 0;
        //testing/////////////////////////////////////////////////////////////////////////////
        for (int i = 0; i < (int)tstrec.entries.size(); i++) {
                //test vector
                const Node *bmu = som->classify(tstrec.entries[i]->vec);
                int cls = bmu->get_class();

                if (parse_path(tstrec.entries[i]->fname, dir, name))
                        wprintf(L" %s\n", dir);

                //out result
                if (cls == tstrec.entries[i]->cls) {
                        pos++;
                        wprintf(L"%s %d          +  ", name, cls);
                } else
                        wprintf(L"%s %d ", name, cls);
                //out bmu position
                for (int d = 0; d < som->get_dimensionality(); d++)
                        wprintf(L"%.2f ", *(bmu->get_coords() + d));
                wprintf(L"\n");

                //check positives/negatives///////////////////////////////////
                if (tstrec.clsnum.size() <= 2) {
                        if (tstrec.entries[i]->cls == cls) {
                                if (cls == 1)
                                        TP++;
                                else if (cls == 2)
                                        TN++;
                        } else { /////error//////////
                                if (cls == 2 && tstrec.entries[i]->cls == 1)  //ill defined as healthy
                                        FN++;
                                else if (cls == 1 && tstrec.entries[i]->cls == 2)  //healthy defined as ill
                                        FP++;
                        }
                }
                //////////////////////////////////////////////////////////////
        }

        //////////////Se,Sp,...
        if (tstrec.clsnum.size() <= 2) {
                if (TP)
                        wprintf(L"\n   sensitivity: %.2f\n", 100.0f * float(TP) / float(TP + FN));
                if (TN)
                        wprintf(L"   specificity: %.2f\n", 100.0f * float(TN) / float(TN + FP));
                if (TP)
                        wprintf(L"   +predictive: %.2f\n", 100.0f * float(TP) / float(TP + FP));
                if (TN)
                        wprintf(L"   -predictive: %.2f\n", 100.0f * float(TN) / float(TN + FN));
                if (TP || FP || FN || TN)
                        wprintf(L"      accuracy: %.2f\n", 100.0f * float(TP + TN) / float(TP + FN + TN + FP));
        } else  //print number of correct classifications only
                wprintf(L"\n   score: %.2f", 100.0f*(float)pos / float(tstrec.entries.size()));
}

void save_weights(int argc, wchar_t* argv[])
{
        som = new SOM(argv[2]);
        if (som->status() != 0) {
                wprintf(L" failed to load SOM: %d", som->status());
                exit(1);
        }

        for (int w = 0; w < som->get_weights_per_node(); w++) {
                for (int n = 0; n < som->get_nodes_number(); n++) {
                        const Node *pnode = som->get_node(n);
                        const float *pweights = pnode->get_weights();
                        wprintf(L"%g\n", pweights[w]);
                }
        }
}
////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////data loading routines/////////////////////////////////////////////////////////
int read_line(FILE *f, wchar_t *buff, int *c)
{
        wint_t res = 0;
        wchar_t *pbuff = buff;

        while ((short)res != EOF) {
                res = fgetwc(f);
                if (res == 0xD || res == 0xA) {
                        if (pbuff == buff) continue;

                        *pbuff = 0;
                        if (!c) {
                                return 1;
                        } else {
                                int ptr = (int)wcslen(buff) - 1;

                                while (ptr > 0) {  //skip thru 'spaces'      dir/asds.f ___1__ \n
                                        if (buff[ptr] != 0x20) break;
                                        else ptr--;
                                }
                                while (ptr > 0) {  //skip thru 'clas type'
                                        if (buff[ptr] == 0x20) break;
                                        else ptr--;
                                }

                                if (ptr) {
                                        *c = _wtoi(&buff[ptr+1]);
                                        while (buff[ptr] == 0x20)  //remove blanks from end of string
                                                buff[ptr--] = 0;
                                } else
                                        *c = 0;

                                return 1;
                        }
                }
                if ((short)res != EOF) {
                        *pbuff++ = (char)res;
                }
        }

        return (short)res;
}

/*
    format 1           //data stored in separate files: ECG,FOUR
     file1 [class]
     file2 [class]
     ...

    format 2           //data stored in this file
     file1 [class]
      vec1 ...
     file2 [class]
      vec1 ...
     ...

            */
/*
     read class data to PTSTREC struct
                                        */
void read_class(FILE *fp, PREC rec, int c)
{
        wchar_t ustr[_MAX_PATH], *pstr;
        int res = 1, cls;

        int entrsize = (int)rec->entries.size();   //size from previous read iteration

        while (res > 0) {
                res = read_line(fp, ustr, &cls);
                if (res > 0) {
                        if (c && !cls) //put default if (c=1,2 and cls=0)
                                cls = c;


                        CSignal *sig = new CSignal(ustr);

                        if (sig->N && sig->M) {   //read file FORMAT 1.*
                                if (!vector_length)
                                        vector_length = sig->M;
                                else {
                                        if (vector_length != sig->M) {
                                                wprintf(L"fmt1.*: vector %s (lenght %d) is not equal to vlen: %d", ustr, sig->M, vector_length);
                                                exit(1);
                                        }
                                }

                                for (int j = 0; j < sig->N; j++) {
                                        if (normalization == 4)
                                                sig->nenergy(sig->data[j], vector_length);
                                        if (normalization == 5)
                                                sig->nminmax(sig->data[j], vector_length, 0.1f, 0.9f);

                                        PENTRY entry = new ENTRY;
                                        entry->vec = sig->data[j];
                                        entry->size = vector_length;
                                        swprintf(entry->fname, L"%s_%d", ustr, j);
                                        entry->cls = cls;
                                        rec->entries.push_back(entry);
                                }

                                signals.push_back(sig);
                        }

                        else {  //FORMAT 2
                                //[filename] [class]
                                //samples
                                float tmp;
                                vector<float> fvec;

                                while (fwscanf(fp, L"%f", &tmp) == 1)
                                        fvec.push_back(tmp);

                                if (fvec.size() == 0) {
                                        wprintf(L"fmt2: vector %s has zero lenght", ustr);
                                        exit(1);
                                }

                                if (!vector_length)
                                        vector_length = (int)fvec.size();
                                else {
                                        if (vector_length != (int)fvec.size()) {
                                                wprintf(L"fmt2: vector %s (lenght %d) is not equal to vector_length: %d", ustr, fvec.size(), vector_length);
                                                exit(1);
                                        }
                                }

                                pstr = new wchar_t[_MAX_PATH];
                                wcscpy(pstr, ustr);

                                if (normalization == 4)
                                        sig->nenergy(&fvec[0], vector_length);
                                if (normalization == 5)
                                        sig->nminmax(&fvec[0], vector_length, 0.1f, 0.9f);

                                float *fdata = new float[vector_length];
                                for (int i = 0; i < vector_length; i++)
                                        fdata[i] = fvec[i];

                                PENTRY entry = new ENTRY;
                                entry->vec = fdata;
                                entry->size = vector_length;
                                wcscpy(entry->fname, pstr);
                                entry->cls = cls;
                                rec->entries.push_back(entry);


                                delete sig;
                        }

                }// if(res > 0) line was read from file
        }// while(res > 0)  res = read_line(fp,ustr, &cls);
        fclose(fp);


        //arrange indices of classes
        if ((int)rec->entries.size() > entrsize) {
                //find new classes in entries not in rec->clsnum array
                for (int i = entrsize; i < (int)rec->entries.size(); i++) {
                        int cls = rec->entries[i]->cls;
                        bool match = false;
                        for (int j = 0; j < (int)rec->clsnum.size(); j++) {
                                if (cls == rec->clsnum[j]) {
                                        match = true;
                                        break;
                                }
                        }
                        if (!match) //no match
                                rec->clsnum.push_back(cls);
                }
                //clsnum = [cls 1][cls 2] ... [cls N]   N entries
                //clsnum = [1][2][3] or [3][1][2] or ... may be not sorted


                if (rec->clsnum.size() > rec->indices.size()) {
                        vector<int> indices;
                        int s = (int)(rec->clsnum.size() - rec->indices.size());
                        for (int i = 0; i < s; i++)
                                rec->indices.push_back(indices);
                }
                //arrange indices
                for (int i = 0; i < (int)rec->clsnum.size(); i++) {
                        //fill positions of clsnum[i] class to indices vector
                        for (int j = entrsize; j < (int)rec->entries.size(); j++) {
                                if (rec->clsnum[i] == rec->entries[j]->cls)
                                        rec->indices[i].push_back(j);
                        }
                }
        }
}
//////////////////data loading routines/////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////
void get_file_name(wchar_t *path, wchar_t *name)
{
        int sl = 0, dot = (int)wcslen(path);
        int i;
        for (i = 0; i < (int)wcslen(path); i++) {
                if (path[i] == '.') break;
                if (path[i] == '\\') break;
        }
        if (i >= (int)wcslen(path)) {
                wcscpy(name, path);
                return;
        }

        for (i = (int)wcslen(path) - 1; i >= 0; i--) {
                if (path[i] == '.')
                        dot = i;
                if (path[i] == '\\') {
                        sl = i + 1;
                        break;
                }
        }

        memcpy(name, &path[sl], (dot - sl)*2);
        name[dot-sl] = 0;
}

int parse_path(wchar_t *path, wchar_t *dir, wchar_t *name)   //true if dirs equal
{
        int res;
        int i;
        for (i = (int)wcslen(path) - 1; i > 0; i--) {
                if (path[i] == '\\')
                        break;
        }

        if (i) { //path + name
                wcscpy(name, &path[i+1]);
                path[i] = 0;
                res = wcscmp(dir, path);
                wcscpy(dir, path);
        } else { //no path
                res = wcscmp(dir, L"");
                wcscpy(dir, L"");
                wcscpy(name, path);
        }
        return res;   //res=0 if dir and path\filename are equal
}
//////////////////////////////////////////////////////////////////////////////////////////

void msec_to_time(int msec, int& h, int& m, int& s, int& ms)
{
        ms = msec % 1000;
        msec /= 1000;

        if (msec < 60) {
                h = 0;
                m = 0;
                s = msec;                 //sec to final
        } else {
                float tmp;
                tmp = (float)(msec % 60) / 60;
                tmp *= 60;
                s = int(tmp);
                msec /= 60;

                if (msec < 60) {
                        h = 0;
                        m = msec;
                } else {
                        h = msec / 60;
                        tmp = (float)(msec % 60) / 60;
                        tmp *= 60;
                        m = int(tmp);
                }
        }
}
