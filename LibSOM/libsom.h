

#ifndef Som_h
#define Som_h


//class Node;  before inlined
#include "node.h"

class SOM
{
public:
        SOM(int dimensionality, int *dimensions, int weights_per_node,          //create/randomize nodes with weights        
            enum Node::DistanceMetric distance_metric, float *add = 0, float *mul = 0);     
        SOM(const wchar_t *fname);                                              //load SOM net, if empty, default norm=0
        ~SOM();

        enum Normalization {NONE, MNMX, ZSCR, SIGM, ENRG};
        enum TrainMode {SLOW, FAST};

        static const wchar_t g_distance[5][5];
        static const wchar_t g_normalization[5][5];
        static const wchar_t g_trainmode[2][5];

// Operators
        //const SOM& operator=(const SOM& som);

// Operations
        //training
        float R0() const;                               //calculate initial R0 for 1st epoch
        void train(const vector<float *> *vectors, 
                   float R, float learning_rule);       //train SOM for 1 epoch
        void vote_nodes_from(PREC rec);                 //voting scheme; get_bmu_node to a vector BMU
        void assign_nodes_to(PREC rec);                 //direct scheme; best vector to a node

        //testing
        const Node *classify(const float *vec);         //return BMU node

        int save(const wchar_t *fname) const;
        int save_2D_distance_map(const wchar_t *fname) const;

        void compute_normalization(PREC rec, enum Normalization norm);

// Access
// Inquiry
        inline int status(void) const; 
        inline int get_dimensionality(void) const;       
        inline int get_weights_per_node(void) const; 
        inline int get_nodes_number() const; 
        inline const Node* get_node(int n) const; 
        inline void set_distance_metric(enum Node::DistanceMetric distance_metric);                   
        inline void set_train_mode(enum TrainMode train_mode);                                              

private:
        SOM(const SOM& som);
        const SOM& operator=(const SOM& som);                

        int m_status;                   //SOM status after loading

        int m_dimensionality;           //dimension of SOM
        int *m_dimensions;              //dimensions sizes
        int m_weights_per_node;         //number of weights per node

        vector<Node *> m_nodes;         //array of nodes

        enum Normalization m_normalization;                     //normalization type
        enum Node::DistanceMetric m_distance_metric;            //distance metric used in training
        enum TrainMode m_train_mode;                            //slow/fast mode training

        float *m_data;                  //data vector to classify
        float *m_add;                   //normalization params for input data   (x+add)*mul
        float *m_mul;                   //normalization params for input data


        void create_nodes(const float *add = 0, const float *mul = 0);    //random weights for nodes init to m_nodes array
        const float* normalize(const float *vec);

        Node *get_bmu_node(const float *vec);                  //get best node (neuron that fire)
        Node *get_bmu_0node(const float *vec);                 //get best node if its m_class=0 (unclassified neuron)

        // hold the memory for host version of the 3D map
        bool m_computeOnGpu;
        float * m_hostMap;
        cudaArray_t m_devMapPtr;

};

inline int SOM::status(void) const
{
        return m_status;
}        

inline int SOM::get_dimensionality(void) const
{
        return m_dimensionality;
}          

inline int SOM::get_weights_per_node(void) const
{
        return m_weights_per_node;
}        

inline int SOM::get_nodes_number() const
{
        return (int)m_nodes.size();
}

inline const Node* SOM::get_node(int n) const
{
        return m_nodes[n];
}

inline void SOM::set_distance_metric(enum Node::DistanceMetric distance_metric) 
{
        m_distance_metric = distance_metric;
}

inline void SOM::set_train_mode(enum TrainMode train_mode) 
{
        m_train_mode = train_mode;
}

inline Node *SOM::get_bmu_node(const float *vec)
{
        Node *pbmu_node = m_nodes[0];
        float mindist = pbmu_node->get_distance(vec, m_distance_metric);
        float dist;

        for (int i = 1; i < get_nodes_number(); i++) {
                if ((dist = m_nodes[i]->get_distance(vec, m_distance_metric)) < mindist) {
                        mindist = dist;
                        pbmu_node = m_nodes[i];
                }
        }

        return pbmu_node;
}

inline Node *SOM::get_bmu_0node(const float *vec)
{
        Node *pbmu_0node = 0;
        int n;
        for (n = 0; n < get_nodes_number(); n++) {
                if (m_nodes[n]->get_class() == 0) {
                        pbmu_0node = m_nodes[n];
                        break;
                }
        }

        if (pbmu_0node != 0) { //there is 0class node m_nodes[n]
                float mindist = pbmu_0node->get_distance(vec, m_distance_metric), dist;
                for (int i = n + 1; i < get_nodes_number(); i++) {
                        if (m_nodes[i]->get_class() == 0 &&
                            (dist = m_nodes[i]->get_distance(vec, m_distance_metric)) < mindist) {
                                mindist = dist;
                                pbmu_0node = m_nodes[i];
                        }
                }
                return pbmu_0node;
        } else
                return 0;
}


#endif