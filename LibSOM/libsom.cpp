

#include "stdafx.h"
#include "libsom.h"
//#include "node.h"   before inlined

#include "common.h"

const wchar_t SOM::g_distance[5][5] = {L"Eucl", L"Sosd", L"Txcb", L"Angl", L"Mhln"};
const wchar_t SOM::g_normalization[5][5] = {L"None", L"Mnmx", L"Zscr", L"Sigm", L"Enrg"};
const wchar_t SOM::g_trainmode[2][5] = {L"Slow", L"Fast"};


///////////////////////////////////SOM  constructor/destructor///////////////////////////////////////////
SOM::SOM(int dimensionality, int *dimensions, int weights_per_node,  //example: 3 [10,30,20] 49
         enum Node::DistanceMetric distance_metric, 
         float *add, float *mul) : m_status(1),
                                   m_dimensionality(dimensionality), m_dimensions(0), m_weights_per_node(weights_per_node),   
                                   m_distance_metric(distance_metric), m_normalization(NONE), m_add(0), m_mul(0), m_data(0),
                                   m_train_mode(SLOW)
{       
        m_dimensions = (int *)malloc(m_dimensionality * sizeof(int));
        for (int d = 0; d < m_dimensionality; d++)
                m_dimensions[d] = dimensions[d];

        m_data = (float *)malloc(weights_per_node * sizeof(float));

        create_nodes(add, mul);
}

/*
   status = 0 if complete file format was loaded
   status = 1 if reduced file format was loaded, random weights
   status = -1 if failed to load any file format
*/
SOM::SOM(const wchar_t *fname): m_status(-1), m_dimensionality(0), m_dimensions(0), m_weights_per_node(0), 
                                m_normalization(NONE), m_distance_metric(Node::EUCL), m_train_mode(SLOW), 
                                m_add(0), m_mul(0), m_data(0), m_computeOnGpu(false)
                
{
        wchar_t str[_MAX_PATH] = L"";       

        FILE *fp = _wfopen(fname, L"rt");
        if (fp != 0) {
                if (fwscanf(fp, L"%d", &m_dimensionality) != 1)
                        return;

                int nodes_number = 0; 
                m_dimensions = (int *)malloc(m_dimensionality * sizeof(int));
                for (int i = 0; i < m_dimensionality; i++) {
                        if (fwscanf(fp, L"%d", &m_dimensions[i]) != 1) {
                                fclose(fp);
                                return;
                        }
                        if (i != 0) 
                                nodes_number *= m_dimensions[i];
                        else  
                                nodes_number = m_dimensions[i];
                }

                if (fwscanf(fp, L"%d", &m_weights_per_node) != 1) {
                        fclose(fp);
                        return;
                }
                //m_dimensionality,m_dimensions,m_weights_per_node arranged////////////////////////////

                m_data = (float *)malloc(get_weights_per_node() * sizeof(float));

                //check for distance
                if (fwscanf(fp, L"%s", str) != 1) {
                        create_nodes();
                        m_status = 1;
                        fclose(fp);
                        return;
                }

                //default eucl=0
                if (_wcsicmp(str, g_distance[Node::EUCL]) == 0)
                        m_distance_metric = Node::EUCL;
                else if (_wcsicmp(str, g_distance[Node::SOSD]) == 0)
                        m_distance_metric = Node::SOSD;
                else if (_wcsicmp(str, g_distance[Node::TXCB]) == 0)
                        m_distance_metric = Node::TXCB;
                else if (_wcsicmp(str, g_distance[Node::ANGL]) == 0)
                        m_distance_metric = Node::ANGL;

                //check for classes info
                vector<int> classes;
                for (int i = 0; i < nodes_number; i++) {
                        int cls;
                        if (fwscanf(fp, L"%d", &cls) != 1)
                                break;
                        classes.push_back(cls);
                }

                if ((int)classes.size() == nodes_number) {  //load positions/weights
                        //if classes marks present but norm is absent m_status=-1
                        if (fwscanf(fp, L"%s", str) != 1) {
                                fclose(fp);
                                return;
                        }
                        if (_wcsicmp(str, g_normalization[NONE]) == 0)
                                m_normalization = NONE;
                        else if (_wcsicmp(str, g_normalization[MNMX]) == 0)
                                m_normalization = MNMX;
                        else if (_wcsicmp(str, g_normalization[ZSCR]) == 0)
                                m_normalization = ZSCR;
                        else if (_wcsicmp(str, g_normalization[SIGM]) == 0)
                                m_normalization = SIGM;
                        else if (_wcsicmp(str, g_normalization[ENRG]) == 0)
                                m_normalization = ENRG;

                        m_add = (float *)malloc(m_weights_per_node * sizeof(float));
                        m_mul = (float *)malloc(m_weights_per_node * sizeof(float));
                        for (int i = 0; i < m_weights_per_node; i++) {
                                if (fwscanf(fp, L"%g %g", &m_add[i], &m_mul[i]) != 2) {
                                        fclose(fp);
                                        return;
                                }
                        }

                        //if classes marks present but coords/weights corrupted m_status=-1
                        float *weights = (float *)malloc(m_weights_per_node * sizeof(float));
                        float *coords = (float *)malloc(m_dimensionality * sizeof(float));

                        for (int i = 0; i < nodes_number; i++) {
                                for (int p = 0; p < m_dimensionality; p++)
                                        if (fwscanf(fp, L"%g", &coords[p]) != 1) {
                                                fclose(fp);
                                                return;
                                        }
                                for (int w = 0; w < m_weights_per_node; w++)
                                        if (fwscanf(fp, L"%g", &weights[w]) != 1) {
                                                fclose(fp);
                                                return;
                                        }

                                Node *pnode = new Node(weights, m_weights_per_node, coords, m_dimensionality, classes[i]);
                                m_nodes.push_back(pnode);
                        }

                        free(weights);
                        free(coords);

                        fclose(fp);
                        m_status = 0;                        
                } else {        //corrupted classes
                        fclose(fp);
                        return;
                }
        }
}

SOM::~SOM()
{
        for (int i = 0; i < get_nodes_number(); i++)
                delete m_nodes[i];

        if (m_dimensions != 0)
                free(m_dimensions);
        if (m_add != 0)
                free(m_add);
        if (m_mul != 0)
                free(m_mul);
        if (m_data != 0)
                free(m_data);

        HANDLE_ERROR(cudaFreeArray(m_devMapPtr));
}
///////////////////////////////////SOM  constructor/destructor///////////////////////////////////////////




/////////////////////////////init nodes to random weights///////////////////////////////////
void SOM::create_nodes(const float *add, const float *mul)
{
    m_computeOnGpu = m_dimensionality == 2;

    //normalization parameters////////////////////////////////////
    m_add = (float *)malloc(m_weights_per_node * sizeof(float));
    if (add != 0)
            for (int w = 0; w < m_weights_per_node; w++)
                    m_add[w] = add[w];
    else
            for (int w = 0; w < m_weights_per_node; w++)
                    m_add[w] = 0.0f;

    m_mul = (float *)malloc(m_weights_per_node * sizeof(float));
    if (mul != 0)
            for (int w = 0; w < m_weights_per_node; w++)
                    m_mul[w] = mul[w];
    else
            for (int w = 0; w < m_weights_per_node; w++)
                    m_mul[w] = 1.0f;


    //weights and positions/////////////////////////////////////////
    float *weights = (float *)malloc(m_weights_per_node * sizeof(float));
    float *coords = (float *)malloc(m_dimensionality * sizeof(float));
    for (int i = 0; i < m_dimensionality; i++)
            coords[i] = 0.0f;

    //allocate host memory
    m_hostMap = (float *) malloc(m_dimensions[0] * m_dimensions[1] * m_weights_per_node * sizeof(float));
    
    int width = m_dimensions[0];
    int height = m_dimensions[1];
    int depth = m_weights_per_node;

    //fill coords with all posible combinations
    srand((unsigned int)time(0));
    while (true) {
            //randomize weights
            for (int i = 0; i < m_weights_per_node; i++) {
                    int w = 0xFFF & rand();
                    w -= 0x800;
                    weights[i] = (float)w / 4096.0f;
            }

            Node *pnode = new Node(weights, m_weights_per_node, coords, m_dimensionality);
            m_nodes.push_back(pnode);

            int i = (int) coords[0];
            int j = (int) coords[1];

            memcpy(m_hostMap + i * width * depth + j * height, weights, m_weights_per_node * sizeof(float));

            //increment counter
            for (int i = 0; i < m_dimensionality; i++) {
                    coords[i]++;
                    if (coords[i] >= m_dimensions[i])
                            coords[i] = 0.0f;
                    else
                    {
                        break;
                    }
            }

            float sum = 0.0f;
            for (int i = 0; i < m_dimensionality; i++)
                    sum += coords[i];
            if (sum == 0.0f) 
                    break;
    }

    free(weights);
    free(coords);

    // allocate the array to hold the data on the device
    cudaExtent extent = make_cudaExtent(m_dimensions[0], m_dimensions[1], m_weights_per_node);
    cudaChannelFormatDesc desc = cudaCreateChannelDesc<float>();
    HANDLE_ERROR(cudaMalloc3DArray(&m_devMapPtr, &desc, extent, cudaArraySurfaceLoadStore));

    // copy memory to cuda
    cudaMemcpy3DParms p = {0};
    p.dstArray = m_devMapPtr;
    p.srcPtr = make_cudaPitchedPtr((void *) m_hostMap, extent.width * sizeof(float), extent.width, extent.height);
    p.extent = extent;
    p.kind = cudaMemcpyHostToDevice;
    HANDLE_ERROR(cudaMemcpy3D(&p));
}


////////////////////////////////////////////////////////////////////////////////////////////
/*
      [SOM file format]

       dimensionality
       dim1 dim2 dim3 ...
       weight_per_node
       distance_metric

       classes

       normalization params

       weights coords/coeffs
       node coord 0 0 0 ... dim
        weights
       node coord 1 0 0 ... dim
        weights
       node coord 2 0 0 ... dim
        weights
       ...
                                        */
int SOM::save(const wchar_t *fname) const
{        
        FILE *fp = _wfopen(fname, L"wt");
        if (fp != 0) {
                fwprintf(fp, L"%d\n", m_dimensionality);
                for (int i = 0; i < m_dimensionality; i++)
                        fwprintf(fp, L"%d ", m_dimensions[i]);
                fwprintf(fp, L"\n%d\n%s\n\n", m_weights_per_node, g_distance[m_distance_metric]);

                //save classes numbers of nodes
                for (int n = 0; n < get_nodes_number(); n++) {
                        Node *pnode = m_nodes[n];
                        fwprintf(fp, L" %d", pnode->get_class());

                        if (!((n + 1) % m_dimensions[0]))
                                fwprintf(fp, L"\n");
                }
                fwprintf(fp, L"\n");

                //save norm parameters
                fwprintf(fp, L"%s\n", g_normalization[m_normalization]);
                for (int i = 0; i < m_weights_per_node; i++)
                        fwprintf(fp, L"%g\t\t%g\n", m_add[i], m_mul[i]);
                fwprintf(fp, L"\n");

                //save nodes positions and weights
                for (int n = 0; n < get_nodes_number(); n++) {
                        Node *pnode = m_nodes[n];                        
                        for (int c = 0; c < m_dimensionality; c++)
                                fwprintf(fp, L"%g ", pnode->m_coords[c]);
                        fwprintf(fp, L"\n");

                        for (int w = 0; w < m_weights_per_node; w++)
                                fwprintf(fp, L"%g\n", pnode->m_weights[w]);
                        fwprintf(fp, L"\n");
                }

                fclose(fp);
                return 0;
        } else
                return -1;
}

int SOM::save_2D_distance_map(const wchar_t *fname) const
{
        int D = 2;
        float min_dist = 1.5f;

        if (get_dimensionality() != D)
                return -1;

        FILE *fp = _wfopen(fname, L"wt");
        if (fp != 0) {
                int n = 0;
                for (int i = 0; i < m_dimensions[0]; i++) {
                        for (int j = 0; j < m_dimensions[1]; j++) {                                
                                float dist = 0.0f;
                                int nodes_number = 0;
                                const Node *pnode = get_node(n++);
                                for (int m = 0; m < get_nodes_number(); m++) {
                                        const Node *node = get_node(m);
                                        if (node == pnode)
                                                continue;
                                        float tmp = 0.0;
                                        for (int x = 0; x < D; x++)
                                                tmp += pow(*(pnode->get_coords() + x) - *(node->get_coords() + x), 2.0f);
                                        tmp = sqrt(tmp);
                                        if (tmp <= min_dist) {
                                                nodes_number++;
                                                dist += pnode->get_distance(node->m_weights, m_distance_metric);
                                        }
                                }
                                dist /= (float)nodes_number;
                                fwprintf(fp, L" %f", dist);
                        }
                        fwprintf(fp, L"\n");
                }
                fclose(fp);
                return 0;
        }
        else 
                return -2;
}
////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////normalization//////////////////////////////////////////////////////////
void SOM::compute_normalization(PREC rec, enum Normalization norm)
{        
        m_normalization = norm;

        //calculate disp,mean,min,max
        if (m_normalization == MNMX) {
                for (int x = 0; x < get_weights_per_node(); x++) {
                        m_add[x] = FLT_MAX;     //min
                        m_mul[x] = -FLT_MAX;    //max
                }
                for (int y = 0; y < (int)rec->entries.size(); y++) {
                        if (rec->entries[y] == 0)
                                continue;
                        for (int x = 0; x < get_weights_per_node(); x++) {
                                if (m_add[x] > rec->entries[y]->vec[x]) m_add[x] = rec->entries[y]->vec[x];   //min
                                if (m_mul[x] < rec->entries[y]->vec[x]) m_mul[x] = rec->entries[y]->vec[x];   //max
                        }
                }
                for (int x = 0; x < get_weights_per_node(); x++) {
                        float mul, add;
                        add = -m_add[x];
                        if ((m_mul[x] - m_add[x]) != 0.0f)
                                mul = 1.0f / (m_mul[x] - m_add[x]);
                        else
                                mul = 1.0f;
                        m_add[x] = add;
                        m_mul[x] = mul;
                }
        } else if (m_normalization == ZSCR || m_normalization == SIGM) {                                
                for (int x = 0; x < get_weights_per_node(); x++) {
                        m_add[x] = 0.0f;
                        m_mul[x] = 0.0f;
                }

                //mean values
                int size = 0;
                for (int y = 0; y < (int)rec->entries.size(); y++) {
                        if (rec->entries[y] == 0)
                                continue;
                        for (int x = 0; x < get_weights_per_node(); x++)                                 
                                m_add[x] += rec->entries[y]->vec[x];                        
                        size++;
                }
                for (int x = 0; x < get_weights_per_node(); x++)   //m_add = mean values
                        m_add[x] /= (float)size;

                //dispersions                
                for (int y = 0; y < (int)rec->entries.size(); y++) {
                        if (rec->entries[y] == 0)
                                continue;
                        for (int x = 0; x < rec->entries[y]->size; x++)                                 
                                m_mul[x] += (rec->entries[y]->vec[x] - m_add[x]) * (rec->entries[y]->vec[x] - m_add[x]);                        
                }

                //m_add; m_mul
                for (int x = 0; x < get_weights_per_node(); x++) { //m_add = -mean values;  m_mul = 1/disp
                        float disp = sqrt(m_mul[x] / float(size - 1));
                        if (disp != 0.0f)
                                m_mul[x] = 1.0f / disp;
                        else
                                m_mul[x] = 1.0f;
                        m_add[x] = -m_add[x];
                }
        } 
}

const float* SOM::normalize(const float *vec)
{
        switch (m_normalization) {
        default:
        case NONE:
                for (int x = 0; x < get_weights_per_node(); x++)
                        m_data[x] = vec[x];
                break;

        case MNMX:
                for (int x = 0; x < get_weights_per_node(); x++)
                        m_data[x] = (0.9f - 0.1f) * (vec[x] + m_add[x]) * m_mul[x] + 0.1f;                
                break;

        case ZSCR:
                for (int x = 0; x < get_weights_per_node(); x++)
                        m_data[x] = (vec[x] + m_add[x]) * m_mul[x];                
                break;

        case SIGM:
                for (int x = 0; x < get_weights_per_node(); x++)
                        m_data[x] = 1.0f / (1.0f + exp(-((vec[x] + m_add[x]) * m_mul[x])));                
                break;

        case ENRG:                
                float energy = 0.0f;
                for (int x = 0; x < get_weights_per_node(); x++)
                        energy += vec[x] * vec[x];
                energy = sqrt(energy);

                for (int x = 0; x < get_weights_per_node(); x++)
                        m_data[x] = vec[x] / energy;                
                break;
        }
        return m_data;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////





/////////////////////calculate initial R0 for 1st epoch/////////////////////////////////////////
float SOM::R0() const
{
        float R = 0.0f;

        for (int i = 0; i < m_dimensionality; i++)
                if ((float)m_dimensions[i] > R)
                        R = (float)m_dimensions[i];

        return R / 2.0f;
}
////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////training///////////////////////////////////////////////////////////////
void SOM::train(const vector<float *> *vectors, float R, float learning_rule)   
{

    for (int n = 0; n < (int)vectors->size(); n++) {
        const float *pdata = normalize(vectors->at(n));
                        
        //get best node
        Node *bmu_node = get_bmu_node(pdata);
        const float *p1 = bmu_node->get_coords();                

        if (R <= 1.0f)  //adjust BMU node only
                bmu_node->train(pdata, learning_rule);
        else {
            for (int i = 0; i < get_nodes_number(); i++) { //adjust weights within R
                    const float *p2 = m_nodes[i]->get_coords();
                    float dist = 0.0f;

                    for (int p = 0; p < m_dimensionality; p++)     //dist = sqrt((x1-y1)^2 + (x2-y2)^2 + ...)  distance to node
                            dist += (p1[p] - p2[p]) * (p1[p] - p2[p]);
                    dist = sqrt(dist);

                    if (m_train_mode == FAST && dist > R) 
                            continue;

                    float y = exp(-(1.0f * dist * dist) / (R * R));
                    m_nodes[i]->train(pdata, learning_rule * y);
            }
        }
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//////VOTING scheme best node to a vector
void SOM::vote_nodes_from(PREC rec)
{
        //rec->clsnum = [cls 1][cls 2] ... [cls N]   N entries   examle: 0,1,2  3,1,2   1,4,10 missed classes

        //clear votes for classes of all nodes
        for (int n = 0; n < get_nodes_number(); n++)
                m_nodes[n]->clear_votes((int)rec->clsnum.size());

        while (true) { //untill unclassified nodes m_class=0
                //add vote to a best node for a given class
                for (int y = 0; y < (int)rec->entries.size(); y++) {
                        if (rec->entries[y] == 0)
                                continue;

                        const float *pdata = normalize(rec->entries[y]->vec);
                        Node *pbmu_0node = get_bmu_0node(pdata);
                        
                        //no more m_class=0 nodes
                        if (pbmu_0node == 0) 
                                return;  

                        //check class location in REC->clsnum[] array
                        int c = rec->entries[y]->cls;
                        for (int i = 0; i < (int)rec->clsnum.size(); i++) {
                                if (rec->clsnum[i] == c) {
                                        c = i;
                                        break;
                                }
                        }

                        pbmu_0node->add_vote(c);
                }

                //assign class from the max number of votes for a class
                for (int n = 0; n < get_nodes_number(); n++) {
                        if (m_nodes[n]->get_class() == 0)
                                m_nodes[n]->evaluate_class(&rec->clsnum[0], (int)rec->clsnum.size());
                }
        }
}

//////DIRECT scheme best vector to a node
void SOM::assign_nodes_to(PREC rec)
{
        //run thru nodes and get best vector matching
        for (int n = 0; n < get_nodes_number(); n++) {
                m_nodes[n]->clear_votes();
                
                int index = 0;
                float mindist = FLT_MAX, dist;
                for (int i = 0; i < (int)rec->entries.size(); i++) {
                        if (rec->entries[i] == 0)
                                continue;
                        const float *pdata = normalize(rec->entries[i]->vec);
                        if ((dist = m_nodes[n]->get_distance(pdata, m_distance_metric)) < mindist) {
                                mindist = dist;
                                index = i;
                        }
                }

                m_nodes[n]->set_class(rec->entries[index]->cls);
        }
}
/////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////testing///////////////////////////////////////////////////////
const Node *SOM::classify(const float *vec)
{
        Node *pbmu_node = m_nodes[0];
        const float *pdata = normalize(vec);
        float mindist = pbmu_node->get_distance(pdata, m_distance_metric), dist;
        for (int n = 1; n < get_nodes_number(); n++) {
                if ((dist = m_nodes[n]->get_distance(pdata, m_distance_metric)) < mindist) {
                        mindist = dist;
                        pbmu_node = m_nodes[n];
                }
        }
        return pbmu_node;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
