#pragma once

#include "analysis/linear_order.h"
#include "ir/inst.h"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace analysis {

struct LiveRange {
    int from{0};
    int to{0};
};

struct LiveInterval {
    ir::SSAValue *value{nullptr};
    std::vector<LiveRange> ranges;

    int start() const {
        return ranges.empty() ? 0 : ranges.front().from;
    }
    int end() const {
        int e = 0;
        for (auto &r : ranges) {
            e = std::max(e, r.to);
        }
        return e;
    }

    bool covers(int pos) const {
        for (auto &r : ranges) {
            if (pos < r.from) {
                return false;
            }
            if (r.from <= pos && pos < r.to) {
                return true;
            }
        }
        return false;
    }

    void addRange(int from, int to) {
        if (from >= to) {
            return;
        }
        LiveRange nr{from, to};
        ranges.push_back(nr);
        std::sort(ranges.begin(), ranges.end(), [](const LiveRange &a, const LiveRange &b) {
            if (a.from != b.from) {
                return a.from < b.from;
            }
            return a.to < b.to;
        });

        std::vector<LiveRange> merged;
        merged.reserve(ranges.size());
        for (auto &r : ranges) {
            if (merged.empty() || merged.back().to < r.from) {
                merged.push_back(r);
            } else {
                merged.back().to = std::max(merged.back().to, r.to);
            }
        }
        ranges.swap(merged);
    }

    void setFrom(int from) {
        for (auto &r : ranges) {
            if (r.to <= from) {
                continue;
            }
            if (r.from <= from && from < r.to) {
                r.from = from;
                break;
            }
            if (from < r.from) {
                break;
            }
        }
        if (ranges.empty()) {
            addRange(from, from + 1);
        }
        std::sort(ranges.begin(), ranges.end(), [](const LiveRange &a, const LiveRange &b) { return a.from < b.from; });
        std::vector<LiveRange> merged;
        for (auto &r : ranges) {
            if (merged.empty() || merged.back().to < r.from) {
                merged.push_back(r);
            } else {
                merged.back().to = std::max(merged.back().to, r.to);
            }
        }
        ranges.swap(merged);
    }

    std::optional<LiveRange> rangeCovering(int pos) const {
        for (auto &r : ranges) {
            if (pos < r.from)
                return std::nullopt;
            if (r.from <= pos && pos < r.to)
                return r;
        }
        return std::nullopt;
    }
};

struct BlockPosition {
    int from{0};
    int to{0};
};

class LivenessAnalysis {
  private:
    LinearOrder lin_;
    std::unordered_map<const ir::Inst *, int> posOfInst_;
    std::unordered_map<const ir::BasicBlock *, BlockPosition> posOfBlock_;

    std::unordered_map<ir::SSAValue *, LiveInterval> intervals_;
    std::unordered_map<const ir::BasicBlock *, std::unordered_set<ir::SSAValue *>> liveIn_;

    LiveInterval &interval_(ir::SSAValue *v) {
        auto &it = intervals_[v];
        it.value = v;
        return it;
    }

    static bool isPhi_(const ir::Inst *I) {
        return I && I->opcode() == ir::Opcode::PHI_U64;
    }

    static std::vector<ir::Inst *> nonPhiReverse_(ir::BasicBlock *bb) {
        std::vector<ir::Inst *> out;
        if (!bb) {
            return out;
        }
        auto ops = bb->allInsts();
        out.reserve(ops.size());
        for (auto it = ops.rbegin(); it != ops.rend(); ++it) {
            if (!isPhi_(*it)) {
                out.push_back(*it);
            }
        }
        return out;
    }

    static std::vector<ir::PhiInst *> phisInBlock_(ir::BasicBlock *bb) {
        std::vector<ir::PhiInst *> phis;
        if (!bb)
            return phis;
        for (auto *I : bb->allInsts()) {
            if (!isPhi_(I)) {
                break;
            }
            if (auto *P = dynamic_cast<ir::PhiInst *>(I)) {
                phis.push_back(P);
            }
        }
        return phis;
    }

    void assignPositions_() {
        posOfInst_.clear();
        posOfBlock_.clear();

        int cur = 0;
        for (auto *bb : lin_.blocks) {
            BlockPosition bp;
            bp.from = cur;

            for (auto *I : bb->allInsts()) {
                if (isPhi_(I)) {
                    posOfInst_[I] = bp.from;
                }
            }

            int last = bp.from;
            for (auto *I : bb->allInsts()) {
                if (isPhi_(I)) {
                    continue;
                }
                last += 2;
                posOfInst_[I] = last;
            }

            bp.to = last + 2;

            posOfBlock_[bb] = bp;
            cur = bp.to;
        }
    }

  public:
    void run(ir::BasicBlock *entry) {
        intervals_.clear();
        liveIn_.clear();

        lin_.run(entry);
        assignPositions_();

        for (auto itB = lin_.blocks.rbegin(); itB != lin_.blocks.rend(); ++itB) {
            auto *b = *itB;
            if (!b) {
                continue;
            }

            std::unordered_set<ir::SSAValue *> live;

            for (auto *succ : b->successors) {
                if (!succ) {
                    continue;
                }

                auto itLI = liveIn_.find(succ);
                if (itLI != liveIn_.end()) {
                    for (auto *v : itLI->second) {
                        live.insert(v);
                    }
                }

                for (auto *phiI : phisInBlock_(succ)) {
                    for (auto &in : phiI->incomings()) {
                        if (in.first == b && in.second) {
                            live.insert(in.second);
                        }
                    }
                }
            }

            const auto bp = posOfBlock_[b];
            for (auto *v : live) {
                interval_(v).addRange(bp.from, bp.to);
            }

            for (auto *op : nonPhiReverse_(b)) {
                const int pos = posOfInst_[op];

                if (auto *out = op->result()) {
                    interval_(out).setFrom(pos);
                    live.erase(out);
                }

                for (auto &operand : op->operands()) {
                    if (std::holds_alternative<ir::SSAValue *>(operand)) {
                        auto *in = std::get<ir::SSAValue *>(operand);
                        if (!in) {
                            continue;
                        }
                        interval_(in).addRange(bp.from, pos + 1);
                        live.insert(in);
                    }
                }
            }

            for (auto *phi : phisInBlock_(b)) {
                if (auto *out = phi->result()) {
                    live.erase(out);
                }
            }

            auto itLE = lin_.loopEnd.find(b);
            if (itLE != lin_.loopEnd.end()) {
                auto *loopEndBlock = itLE->second;
                const int loopTo = posOfBlock_[loopEndBlock].to;
                for (auto *v : live) {
                    interval_(v).addRange(bp.from, loopTo);
                }
            }

            liveIn_[b] = std::move(live);
        }
    }

    const LinearOrder &linearOrder() const {
        return lin_;
    }

    const std::unordered_map<ir::SSAValue *, LiveInterval> &intervals() const {
        return intervals_;
    }

    const LiveInterval *getInterval(ir::SSAValue *v) const {
        auto it = intervals_.find(v);
        return it == intervals_.end() ? nullptr : &it->second;
    }

    const LiveInterval *getInterval(const ir::Inst *I) const {
        if (!I)
            return nullptr;
        auto *r = I->result();
        return r ? getInterval(r) : nullptr;
    }

    const std::vector<LiveRange> *getRanges(const ir::Inst *I) const {
        auto *li = getInterval(I);
        return li ? &li->ranges : nullptr;
    }

    int positionOf(const ir::Inst *I) const {
        auto it = posOfInst_.find(I);
        return it == posOfInst_.end() ? -1 : it->second;
    }

    BlockPosition blockPosition(const ir::BasicBlock *bb) const {
        auto it = posOfBlock_.find(bb);
        return it == posOfBlock_.end() ? BlockPosition{} : it->second;
    }

    std::vector<ir::SSAValue *> liveValuesAt(const ir::Inst *I) const {
        std::vector<ir::SSAValue *> out;
        const int pos = positionOf(I);
        if (pos < 0) {
            return out;
        }
        for (auto &kv : intervals_) {
            if (kv.second.covers(pos)) {
                out.push_back(kv.first);
            }
        }
        return out;
    }

    std::vector<std::pair<ir::SSAValue *, LiveRange>> liveRangesAt(const ir::Inst *I) const {
        std::vector<std::pair<ir::SSAValue *, LiveRange>> out;
        const int pos = positionOf(I);
        if (pos < 0)
            return out;
        for (auto &kv : intervals_) {
            if (auto r = kv.second.rangeCovering(pos)) {
                out.push_back({kv.first, *r});
            }
        }
        return out;
    }
};

} // namespace analysis