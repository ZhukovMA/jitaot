#pragma once
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace ir {
class BasicBlock;
}

#include "ir/basic_block.h"

namespace analysis {
using ir::BasicBlock;
struct DominatorTree {
    
    std::unordered_map<BasicBlock *, BasicBlock *> idom_map;
    std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> dom_children;

  private:
    
    std::unordered_map<BasicBlock *, int> idx_; 
    std::vector<BasicBlock *> vertex_;          
    std::vector<int> dsf_parent_, sdom_, idom_, ancestor_, label_;
    std::vector<std::vector<int>> cfg_pred_, bucket_;
    int N_ = 0;
    
    void reset_() {
        idx_.clear();
        vertex_.clear();
        vertex_.push_back(nullptr); 
        dsf_parent_.clear();
        dsf_parent_.push_back(0);
        N_ = 0;
    }

    void dfsVisit_(BasicBlock *b, int pidx) {
        if (!b || idx_.count(b))
            return;
        idx_[b] = ++N_;
        vertex_.push_back(b);
        dsf_parent_.push_back(pidx);
        int me = N_;
        for (auto *s : b->successors) {
            if (s && !idx_.count(s))
                dfsVisit_(s, me);
        }
    }
    
    void dfsNumbering(BasicBlock *start) {
        reset_();
        dfsVisit_(start, 0);
    }

    inline void link(int p, int v) {
        ancestor_[v] = p;
    }

    void compress(int v) {
        if (ancestor_[ancestor_[v]] != 0) {
            compress(ancestor_[v]);
            if (sdom_[label_[ancestor_[v]]] < sdom_[label_[v]])
                label_[v] = label_[ancestor_[v]];
            ancestor_[v] = ancestor_[ancestor_[v]];
        }
    }
    int eval(int v) {
        if (ancestor_[v] == 0)
            return label_[v];
        compress(v);
        return (sdom_[label_[ancestor_[v]]] < sdom_[label_[v]])
                   ? label_[ancestor_[v]]
                   : label_[v];
    }

    
    void buildPredecessors() {
        cfg_pred_.assign(N_ + 1, {});
        for (int i = 1; i <= N_; ++i) {
            for (auto *p : vertex_[i]->predecessors) {
                auto it = idx_.find(p);
                if (it != idx_.end() && it->second != i)
                    cfg_pred_[i].push_back(it->second);
            }
            std::sort(cfg_pred_[i].begin(), cfg_pred_[i].end());
            cfg_pred_[i].erase(std::unique(cfg_pred_[i].begin(), cfg_pred_[i].end()), cfg_pred_[i].end());
        }
    }

  public:
    
    void build(BasicBlock *r) {
        idom_map.clear();
        dom_children.clear();
        if (!r)
            return;
        
        dfsNumbering(r); 
        if (N_ == 0)
            return;
        buildPredecessors(); 

        sdom_.assign(N_ + 1, 0);
        idom_.assign(N_ + 1, 0);
        ancestor_.assign(N_ + 1, 0);
        label_.assign(N_ + 1, 0);
        bucket_.assign(N_ + 1, {});
        for (int i = 1; i <= N_; ++i) {
            sdom_[i] = i;
            label_[i] = i;
        }
        
        for (int w = N_; w >= 2; --w) {
            for (int v : cfg_pred_[w]) {
                int u = eval(v);
                if (sdom_[u] < sdom_[w])
                    sdom_[w] = sdom_[u];
            }
            
            bucket_[sdom_[w]].push_back(w);

            link(dsf_parent_[w], w);
            
            auto &B = bucket_[dsf_parent_[w]];
            while (!B.empty()) {
                int v = B.back();
                B.pop_back();
                int u = eval(v);
                
                if (sdom_[u] < sdom_[v])
                    idom_[v] = u;
                else
                    idom_[v] = sdom_[v];
            }
        }

        idom_[1] = 0; 
        for (int w = 2; w <= N_; ++w) {
            
            if (idom_[w] != sdom_[w])
                idom_[w] = idom_[idom_[w]];
        }
        
        for (int i = 1; i <= N_; ++i) {
            BasicBlock *b = vertex_[i];
            BasicBlock *p = (idom_[i] ? vertex_[idom_[i]] : nullptr);
            idom_map[b] = p;
            if (p)
                dom_children[p].push_back(b);
        }
    }
};
} 