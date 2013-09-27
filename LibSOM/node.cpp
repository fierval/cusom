

#include <stdafx.h>
#include "node.h"



///////////////////////////////////node constructor/destructor///////////////////////////////////////////
Node::Node(const float *weights, int weights_number, 
           const float *coords, int coords_number, int class_) : m_weights_number(weights_number), m_class(class_), m_precision(0.0f)
{
        m_weights = (float *)malloc(m_weights_number * sizeof(float));
        for (int i = 0; i < weights_number; i++)
                m_weights[i] = weights[i];

        m_coords = (float *)malloc(coords_number * sizeof(float));
        for (int i = 0; i < coords_number; i++)
                m_coords[i] = coords[i];
}
Node::~Node()
{
        free(m_weights);
        free(m_coords);
}
///////////////////////////////////node constructor/destructor///////////////////////////////////////////



/////////////////////////////votes calculation/////////////////////////////////////////////
void Node::clear_votes(int classes_number)
{
        if (m_votes.size() && classes_number == (int)m_votes.size()) {
                for (int c = 0; c < classes_number; c++)
                        m_votes[c] = 0;
        } else {
                m_votes.clear();
                for (int c = 0; c < classes_number; c++)
                        m_votes.push_back(0);
        }
        m_class = 0;
        m_precision = 0.0f;
}

bool Node::evaluate_class(const int *classes, int classes_number) //classes 1,2,3  or  2,4,5  or 5,2,1 ... not in ascending order
{
        if (classes_number) {
                m_precision = 0.0f;

                //get max votes number and assign a class to that node
                int maxvotes = m_votes[0];
                m_class = classes[0];
                for (int c = 1; c < classes_number; c++) {
                        if (maxvotes < m_votes[c]) {
                                maxvotes = m_votes[c];
                                m_class = classes[c];
                        }
                }

                //calculate node presicion = maxvotes/(cls1votes+cls2votes+ ... )
                if (maxvotes) {
                        for (int c = 0; c < classes_number; c++)
                                m_precision += m_votes[c];
                        m_precision = ((float)maxvotes / m_precision);
                } else
                        m_class = 0;

                return true;
        } else
                return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////
