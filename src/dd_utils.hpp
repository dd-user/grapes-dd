/*
Copyright (c) 2020

GRAPES is provided under the terms of The MIT License (MIT):

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef UTILS_HPP
#define UTILS_HPP

#include <chrono>
#include <iostream>
#include <cstring>
#include <exception>
#include <string>
#include <stdexcept>
#include <sstream>
#include <map>

#include <meddly.h>
#include <meddly_expert.h>

#include "LabelMap.h"

#define DEBUG 0
#define WRITE_MDD_PDF 0


class Parser; 

using time_point = std::chrono::_V2::steady_clock::time_point; 


inline double get_time_interval(time_point end_t, time_point start_t) {
    return std::chrono::duration_cast<std::chrono::duration<double>>(end_t - start_t).count(); 
}

namespace mtdds {
    struct MtddStats;
    class GraphNodeEncoder; 
    class Encoder;


    //it identifies one of the indexed graphs 
    using graph_id_t = unsigned int; 
    //it identifies one node of a specific graph
    using node_id_t = unsigned int; 
    //it represent a specific node of a specific graph 
    using graph_node_t = std::pair<graph_id_t, node_id_t>; 
    //vector of integers used to initialize the domain of MDDs
    using domain_bounds_t = std::vector<int>;
    //vector of strings used to perform path queries on MDDs 
    using label_path_t = std::vector<std::string>; 
    //(graph_id + node_id infos) + num_occurrences 
    using single_result_t = std::pair<graph_node_t, int>; 
    //vector of single query results 
    using query_result_t = std::vector<single_result_t>; 

    struct MtddStats {
        long num_vars = 0; //number of mtdd levels
        long num_graphs = 0;  //number of indexed graphs
        long num_labels = 0;  //number of labels
        long num_nodes = 0; //current number of nodes 
        long peak_nodes = 0; //peak number of nodes reached during mtdd building 
        long memory_used = 0; //current memory required to store mtdd 
        long peak_memory = 0;  //peak memory utilization reached during mtdd building
        long num_edges = 0;  //current number of edges 
        long num_unique_nodes = 0; //current number of unique nodes 
        long cardinality = 0; //number of stored elements 

        void show() {
            std::cout << "##################\n"
                << "num_vars = " << num_vars << "\n"
                << "num_graphs = " << num_graphs << "\n"
                << "num_labels = " << num_labels << "\n"
                << "num_nodes = " << num_nodes << "\n"
                << "memory_used = " << memory_used << "\n"
                << "cardinality = " << cardinality << std::endl; 
        }
    }; 

    /** GraphNodeEncoder maps graph nodes to a single numeric value */
    class GraphNodeEncoder : public std::map<graph_node_t, unsigned> {
        using base = std::map<graph_node_t, unsigned>; 
        using gn_pair = std::pair<graph_node_t, unsigned>;
        
        //easy way to know the number of vertex of each indexed graph 
        std::map<graph_id_t, size_t> nodes_per_graph; 
        //mapping from integers to the proper pair (graph_id, node_id)
        std::vector<graph_node_t> inverse_mapping; 
        //total number of nodes in the indexed graphs 
        size_t tot_nodes; 

    public: 
        
        GraphNodeEncoder() : base(), tot_nodes(0) {}
        ~GraphNodeEncoder () {}

        void show() const { 
            for (auto it = begin(); it != end(); ++it) {
                std::cout << "(" << it->first.first << " " << it->first.second << ") => " << it->second << std::endl; 
            } 
        }

        inline const graph_node_t& inverse_map(unsigned vmapped) const {
            return inverse_mapping.at(vmapped); 
        }

        //it maps one node of a specific graph to an integer value 
        inline unsigned map(graph_id_t gid, node_id_t nid) { 
            graph_node_t gn(gid, nid);
            base::iterator it = this->find(gn);

            if (it == end()) {
                it = this->insert(std::make_pair(gn, size() + 1)).first; 
                //increment nodes_per_graph counter 
                auto curr = nodes_per_graph.emplace(
                    std::piecewise_construct, 
                    std::forward_as_tuple(gid), 
                    std::forward_as_tuple(0)
                ); 
                ++curr.first->second; 
                ++tot_nodes; 
            }
            
            return it->second;
        } 

        //it returns the integer representing one node of a specific graph 
        inline unsigned get(graph_id_t gid, node_id_t nid) const {
            graph_node_t gn(gid, nid); 
            base::const_iterator it = this->find(gn); 
            return it != end() ? it->second : 0; 
        }

        //it returns the number of nodes of the specified graph  
        inline size_t num_nodes(graph_id_t gid) const {
            auto it = nodes_per_graph.find(gid); 
            return it != nodes_per_graph.end() ? it->second : 0; 
        }

        inline size_t num_graphs() const {
            return nodes_per_graph.size(); 
        }

        //it returns the total number of nodes, considering all the indexed graphs 
        inline size_t num_nodes() const {
            return tot_nodes; 
        }

        void build_inverse_mapping() {
            inverse_mapping.resize(size() + 1); 

            for (auto it = begin(); it != end(); ++it)
                inverse_mapping.at(it->second) = it->first;  
        }
    };  


    class Encoder {
        using mapPair = std::pair<std::string, int>; 
        using mapStringToInt = std::map<std::string, int>; 

        mapStringToInt labelMapping; 

    public: 
        Encoder() {}

        inline Encoder(GRAPESLib::LabelMap& grapesLabelMap) : Encoder() {
            initFromGrapesLabelMap(grapesLabelMap); 
        }

        inline void initFromGrapesLabelMap(GRAPESLib::LabelMap& grapesLabelMap) {
            for (const auto& x: grapesLabelMap) 
                set(x.first, x.second + 1); 
        }

        inline void initGrapesLabelMap(GRAPESLib::LabelMap& grapesLabelMap) const {
            for (const auto& x: labelMapping) {
                grapesLabelMap.emplace(
                    std::piecewise_construct, 
                    std::forward_as_tuple(x.first), 
                    std::forward_as_tuple(x.second - 1)
                ); 
            }
        }

        inline int map(const std::string& key) {
            auto it = labelMapping.find(key); 

            if (it == labelMapping.end()) 
                //adding +1 in order to avoid zero value 
                it = labelMapping.emplace(key, 1 + labelMapping.size()).first; 

            return it->second; 
        }

        inline int get(const std::string& key) {
             auto it = labelMapping.find(key); 
            return it != labelMapping.end() ? it->second : 0;   
        }

        inline void set(const std::string& key, int value) {
            labelMapping.emplace(key, value); 
        }

        inline bool contains(const std::string& key) {
            return labelMapping.find(key) != labelMapping.end();
        }

        inline mapStringToInt::const_iterator begin() {
            return labelMapping.begin(); 
        } 

        inline mapStringToInt::const_iterator end() {
            return labelMapping.end(); 
        } 

        inline size_t size() const {
            return labelMapping.size();  
        }

        inline void show_mapping() {
            for (auto it = labelMapping.begin(); it != labelMapping.end(); ++it)
                std::cout << it->first << " => " << it->second << "; "; 
            std::cout << std::endl; 
        }
    }; 
}


class Parser {
    const size_t buffersize = 256; 

    std::string delimiters; 
    char *buffer; 
    bool state; 
public:
    inline Parser(const char* delimiters) : state(false) {
        buffer = new char[buffersize];
        set_delimiters(delimiters);
    }

    inline ~Parser() {
        delete buffer; 
    }

    inline void set_string(char *str) {
        strncpy(this->buffer, str, buffersize);
        this->state = false; 
    }

    inline void set_delimiters(const char* delimiters) {
        this->delimiters.assign(delimiters); 
    }

    inline char* parse() {
        char *ret = state ? strtok(NULL, delimiters.c_str()) : strtok(buffer, delimiters.c_str()); 
        state = ret != NULL; 
        return ret; 
    }

    inline int parseint() {
        char* p = this->parse();

        if (!p) 
            throw std::invalid_argument("Invalid token"); 

        return atoi(p); 
    }
};

#endif