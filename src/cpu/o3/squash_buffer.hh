#ifndef __CPU_O3_SQUASH_BUFFER_HH__
#define __CPU_O3_SQUASH_BUFFER_HH__

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/statistics.hh"
#include "base/types.hh"
#include "cpu/base_dyn_inst.hh"
#include "cpu/inst_seq.hh"
#include "cpu/o3/bloom_filter.hh"
#include "cpu/o3/counting.hh"
#include "cpu/o3/hash.hh"

struct DerivO3CPUParams;
template <class Impl>
class FullO3CPU;

using bf::counting_bloom_filter;
using bf::make_hasher;

template <class Impl>
class BaseSquashBuffer {
   public:
    typedef typename Impl::DynInst DynInst;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::O3CPU O3CPU;

    BaseSquashBuffer(O3CPU *cpu, size_t max_size)
        : _cpu(cpu), _max_size(max_size) { regStats(); }

    size_t maxSize() const { return _max_size; }
    virtual bool full() const = 0;
    virtual bool check(DynInstPtr inst) = 0;
    virtual bool clear(DynInstPtr inst) = 0;
    virtual void squash(DynInstPtr inst) = 0;
    virtual void insert(DynInstPtr inst) = 0;
    virtual void retire(DynInstPtr inst) = 0;

   protected:
    O3CPU *_cpu;
    size_t _max_size;

    // Stats
    Stats::Scalar SBChecks;
    Stats::Scalar SBClears;
    Stats::Scalar SBInserts;
    Stats::Scalar SBHits;
    Stats::Scalar SBMisses;
    Stats::Scalar SBOverflows;
    Stats::Scalar FFalsePositives;
    Stats::Scalar FFalseNegatives;
    Stats::Scalar SBSeqChange;
    Stats::Scalar CFFRandReplace;
    Stats::Distribution MaxSBEntries;

    std::string name() const {
        return _cpu->name() + ".squashBuffer";
    }

    void regStats() {
        SBChecks
            .name(name() + ".SBChecks")
            .desc("Number of SB checks");

        SBClears
            .name(name() + ".SBClears")
            .desc("Number of SB clear");

        MaxSBEntries
            .init(0,
                  _max_size,  // value ranges from 0 to _max_size * 2
                  20)         // use 10 buckets to store pdf
            .name(name() + ".MaxSBEntries")
            .desc("Distribution of maximum SB entry#")
            .flags(Stats::pdf);

        SBHits
            .name(name() + ".SBHits")
            .desc("Number of times the SB returned it contained a value");

        SBMisses
            .name(name() + ".SBMisses")
            .desc("Number of times the SB returned it did contained a value");

        SBOverflows
            .name(name() + ".SBOverflows")
            .desc("Number of SB overflows");

        SBInserts
            .name(name() + ".SBInserts")
            .desc("Number of times the a value was inserted in the SB");

        SBSeqChange
            .name(name() + ".SBSeqChange")
            .desc("Number of times the sequence number got reset on clear");

        FFalsePositives
            .name(name() + ".FFalsePositives")
            .desc("Number of times the Filter falsely returned it contained a value");

        FFalseNegatives
            .name(name() + ".FFalseNegatives")
            .desc("Number of times the Filter falsely returned it didn't contain a value");
    }
};

template <class Impl>
class SimpleSquashBuffer : public BaseSquashBuffer<Impl> {
   public:
    typedef typename Impl::DynInst DynInst;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::O3CPU O3CPU;

    SimpleSquashBuffer(O3CPU *cpu, size_t max_size, long long elem_cnt) : BaseSquashBuffer<Impl>(cpu, max_size) {
        _bloom = GCONFIG.sbHW == utils::BLOOM;
        if (_bloom) {
            _parameters.projected_element_count = elem_cnt;  //
            _parameters.false_positive_probability = 0.01;   // 1 in 100
            _parameters.random_seed = 0xA5A5A5A5;            // repeatable results

            if (!_parameters) {
                std::cerr << "Invalid bloom filter parameters" << std::endl;
            }

            _parameters.compute_optimal_parameters();

            blfilter = new bloom_filter(_parameters);

            std::cerr << "Bloom Filter projected element count: "
                      << _parameters.projected_element_count << std::endl;
            std::cerr << "Bloom Filter false positive probability: "
                      << _parameters.false_positive_probability << std::endl;
            std::cerr << "Bloom Filter number of hashes: "
                      << _parameters.optimal_parameters.number_of_hashes << std::endl;
            std::cerr << "Bloom Filter table size: "
                      << _parameters.optimal_parameters.table_size << std::endl;
        }
    }

    bool full() const override {
        if (_bloom)
            return false;
        else
            return _sb.size() >= this->_max_size;
    }

    bool check(DynInstPtr inst) override {
        auto inst_addr = inst->instAddr();
        bool ret = false;
        if (_bloom) {
            ret = blfilter->contains(inst_addr);
            bool ret2 = _sb.find(inst_addr) != _sb.end();
            if (ret != ret2) {
                this->FFalsePositives++;
            }
        } else {
            ret = _sb.find(inst_addr) != _sb.end();
        }
        if (ret) {
            this->SBHits++;
        } else {
            this->SBMisses++;
        }
        return ret;
    }

    bool clear(DynInstPtr inst) override {
        CSPRINT(Try2Clear, inst, "oldest seqNum: %lli\n", _oldest_sq_src);
        auto inst_seq = inst->seqNum;
        if (inst_seq == _oldest_sq_src) {
            this->MaxSBEntries.sample(_sb.size());
            _oldest_sq_src = std::numeric_limits<InstSeqNum>::max();

            if (_bloom) {
                blfilter->clear();
            }
            _sb.clear();
            this->SBClears++;
            return true;
        } else {
            // if the oldest squash source "disappeared"
            if (inst_seq > _oldest_sq_src) {
                this->MaxSBEntries.sample(_sb.size());
                _oldest_sq_src = std::numeric_limits<InstSeqNum>::max();
                if (_bloom) {
                    blfilter->clear();
                }
                _sb.clear();
                this->SBClears++;
                this->SBSeqChange++;
                return true;
            } else {
                return false;
            }
        }
    }

    void squash(DynInstPtr inst) override {
        auto sq_src = inst->seqNum;
        if (sq_src < _oldest_sq_src) {
            _oldest_sq_src = sq_src;
        }
    }

    void insert(DynInstPtr inst) override {
        Addr inst_addr = inst->instAddr();
        if (_bloom) {
            blfilter->insert(inst_addr);
            _sb.insert(inst_addr);
        } else {
            _sb.insert(inst_addr);
        }
        this->SBInserts++;
    }

    void retire(DynInstPtr inst) override {
        assert(false && "Does not support retire");
    }

   private:
    std::unordered_set<Addr> _sb;
    InstSeqNum _oldest_sq_src = std::numeric_limits<InstSeqNum>::max();

    bool _bloom;
    bloom_parameters _parameters;
    bloom_filter *blfilter;
};

template <class Impl>
class EpochSquashBuffer : public BaseSquashBuffer<Impl> {
   public:
    typedef typename Impl::DynInst DynInst;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::O3CPU O3CPU;
    typedef std::unordered_map<Addr, size_t> SquashBuffer;

    EpochSquashBuffer(O3CPU *cpu, size_t max_size, size_t max_active, long long elem_cnt)
        : BaseSquashBuffer<Impl>(cpu, max_size), _max_active(max_active), _elems(elem_cnt),
        _max_counter((1 << GCONFIG.counterSize) - 1) {
        if (GCONFIG.sbHW == utils::BLOOM || GCONFIG.sbHW == utils::COUNTING_BLOOM) {
            _parameters.projected_element_count = elem_cnt;  //
            _parameters.false_positive_probability = 0.01;   // 1 in 100
            _parameters.random_seed = 0xA5A5A5A5;            // repeatable results

            if (!_parameters) {
                std::cerr << "Invalid bloom filter parameters" << std::endl;
            }

            _parameters.compute_optimal_parameters();

            std::cerr << "Bloom Filter projected element count: "
                      << _parameters.projected_element_count << std::endl;
            std::cerr << "Bloom Filter false positive probability: "
                      << _parameters.false_positive_probability << std::endl;
            std::cerr << "Bloom Filter number of hashes: "
                      << _parameters.optimal_parameters.number_of_hashes << std::endl;

            if (!GCONFIG.deleteOnRetire) {
                // change the counting bloom filter to bloom filter
                std::cerr << "Bloom Filter table size: "
                      << _parameters.optimal_parameters.table_size * GCONFIG.counterSize
                      << std::endl;
            }
            else {
                std::cerr << "Bloom Filter table size: "
                          << _parameters.optimal_parameters.table_size << std::endl;
            }
        }

        uint _bucket = (uint)max_active / 10;
        _bucket = std::max(1u, _bucket);
        activeRecords
            .init(0, max_active, _bucket)
            .name(this->name() + ".activeRecords")
            .desc("Number of active epoch records")
            .flags(Stats::pdf);

        SBRetireDeletions
            .name(this->name() + ".SBRetireDeletions")
            .desc("Number of deletions caused by retirement");

        SBCounterOverflows
            .name(this->name() + ".SBCounterOverflows")
            .desc("Number of counter overflows");
    }

    bool full() const override {
        switch (GCONFIG.sbHW) {
            case utils::BLOOM:
                return _bf.size() >= _max_active;
                break;
            case utils::COUNTING_BLOOM:
                return _cbf.size() >= _max_active;
                break;
            case utils::IDEAL:
                return _sb.size() >= _max_active;
                break;
            default:
                panic("Unknown SB structure!");
        }
    }

    bool check(DynInstPtr inst) override {
        Addr inst_addr = inst->instAddr();
        auto epochID = inst->epochID;
        bool found = false, found_set = false;
        bool hitFilter = false;

        if (GCONFIG.checkAllRecords) {
            for (auto &p : _sb) {
                if (IN_MAP(inst_addr, p.second) && p.second.at(inst_addr) > 0) {
                    found_set = true;
                    break;
                }
            }
        } else {
            if (IN_MAP(epochID, _sb) &&
                IN_MAP(inst_addr, _sb.at(epochID)) &&
                _sb.at(epochID).at(inst_addr) > 0) {
                found_set = true;
            }
            if (GCONFIG.sbHW == utils::IDEAL) {
                hitFilter = IN_MAP(epochID, _sb);
            }
        }

        switch (GCONFIG.sbHW) {
            case utils::BLOOM:
                activeRecords.sample(_bf.size());
                if (GCONFIG.checkAllRecords) {
                    for (auto &p : _bf) {
                        if (p.second->contains(inst_addr)) {
                            found = true;
                            break;
                        }
                    }
                } else {
                    if (IN_MAP(epochID, _bf) &&
                        _bf.at(epochID)->contains(inst_addr)) {
                        found = true;
                    }
                    hitFilter = IN_MAP(epochID, _bf);
                }
                break;
            case utils::COUNTING_BLOOM:
                activeRecords.sample(_cbf.size());
                if (GCONFIG.checkAllRecords) {
                    for (auto &p : _cbf) {
                        if (p.second->lookup(inst_addr) > 0) {
                            found = true;
                            break;
                        }
                    }
                } else {
                    if (IN_MAP(epochID, _cbf) &&
                        _cbf.at(epochID)->lookup(inst_addr) > 0) {
                        found = true;
                    }
                    hitFilter = IN_MAP(epochID, _cbf);
                }
                break;
            case utils::IDEAL:
                activeRecords.sample(_sb.size());
                if (GCONFIG.checkAllRecords) {
                    for (auto &p : _sb) {
                        if (IN_MAP(inst_addr, p.second) && p.second.at(inst_addr) > 0) {
                            if (!IN_MAP(p.first, _counterOverflowBuffer) ||
                                !IN_MAP(inst_addr, _counterOverflowBuffer.at(p.first)) ||
                                p.second.at(inst_addr) -
                                _counterOverflowBuffer.at(p.first).at(inst_addr) > 0) {

                                found = true;
                                break;
                            }
                        }
                    }
                }
                else {
                    if (IN_MAP(epochID, _sb) &&
                        IN_MAP(inst_addr, _sb.at(epochID)) &&
                        _sb.at(epochID).at(inst_addr) > 0) {
                        if (!IN_MAP(epochID, _counterOverflowBuffer) ||
                            !IN_MAP(inst_addr, _counterOverflowBuffer.at(epochID)) ||
                            _sb.at(epochID).at(inst_addr) -
                            _counterOverflowBuffer.at(epochID).at(inst_addr) > 0) {

                            found = true;
                        }
                    }
                }
                break;
            default:
                panic("Unknown SB structure!");
        }

        if (found)
            this->SBHits++;
        else
            this->SBMisses++;

        if (found && !found_set)
            this->FFalsePositives++;
        else if (!found && found_set)
            this->FFalseNegatives++;

        if (_ar_overflowed && !hitFilter) {
            // if AR overflows and we do not find a record in SB,
            // then we fence it if epochID <= _overflow_epoch
            return epochID <= _overflowed_epoch;
        }
        else {
            return found;
        }
    }

    bool clear(DynInstPtr inst) override {
        auto epochID = inst->epochID - 1;  // clear prev epochs
        CSPRINT(Try2Clear, inst, "clearing epoch <= $lli\n", epochID);

        if (epochID >= _overflowed_epoch) {
            _overflowed_epoch = 0;
            _ar_overflowed = false;
        }

        std::vector<InstSeqNum> toErase;
        switch (GCONFIG.sbHW) {
            case utils::BLOOM:
                for (auto &p : _bf) {
                    if (epochID >= p.first) {
                        toErase.emplace_back(p.first);
                    }
                }
                for (auto k : toErase) {
                    delete _bf[k];
                    _bf.erase(k);
                }
                this->SBClears += toErase.size();
                break;
            case utils::COUNTING_BLOOM:
                for (auto &p : _cbf) {
                    if (epochID >= p.first) {
                        toErase.emplace_back(p.first);
                    }
                }
                for (auto k : toErase) {
                    delete _cbf[k];
                    _cbf.erase(k);
                }
                break;
            case utils::IDEAL:
                for (auto &p : _counterOverflowBuffer) {
                    if (epochID >= p.first) {
                        toErase.emplace_back(p.first);
                    }
                }
                for (auto k : toErase) {
                    _counterOverflowBuffer.erase(k);
                }
                break;
            default:
                panic("Unknown SB structure!");
        }

        toErase.clear();
        for (auto &p : _sb) {
            if (epochID >= p.first) {
                toErase.emplace_back(p.first);
                this->MaxSBEntries.sample(p.second.size());
            }
        }
        for (auto k : toErase) {
            _sb.erase(k);
        }
        if (GCONFIG.sbHW == utils::IDEAL) {
            this->SBClears += toErase.size();
        }

        return true;
    }


    bool needsNewEntry(uint64_t epochID) {
        switch (GCONFIG.sbHW) {
            case utils::BLOOM:
                return !IN_MAP(epochID, _bf);
                break;
            case utils::COUNTING_BLOOM:
                return !IN_MAP(epochID, _cbf);
                break;
            case utils::IDEAL:
                return !IN_MAP(epochID, _sb);
                break;
            default:
                panic("Unknown SB structure!");
                return false;
        }
    }

    void insert(DynInstPtr inst) override {
        CSPRINT(Insert2Buffer, inst, "remain: %d\n", _sb.size());
        this->SBInserts++;
        auto epochID = inst->epochID;

        if (full() && needsNewEntry(epochID)) {
            this->SBOverflows++;
            _ar_overflowed = true;
            _overflowed_epoch = std::max(_overflowed_epoch, epochID);
            return;
        }

        switch (GCONFIG.sbHW) {
            case utils::BLOOM:
                if (IN_MAP(inst->epochID, _bf)) {
                    _bf.at(inst->epochID)->insert(inst->instAddr());
                } else {
                    _bf[inst->epochID] = new bloom_filter(_parameters);
                    _bf[inst->epochID]->insert(inst->instAddr());
                }
                break;
            case utils::COUNTING_BLOOM:
                if (IN_MAP(inst->epochID, _cbf)) {
                    if (_cbf.at(inst->epochID)->lookup(inst->instAddr()) >= _max_counter) {
                        SBCounterOverflows++;
                    }
                    _cbf.at(inst->epochID)->add(inst->instAddr());
                } else {
                    auto h = make_hasher(_parameters.optimal_parameters.number_of_hashes,
                                         0x5bd1e995, false); // function#, seed, double_hashing

                    if (!GCONFIG.deleteOnRetire) {
                        // change the counting bloom filter to bloom filter
                        _cbf[inst->epochID] = new counting_bloom_filter(
                                              std::move(h),
                                              _parameters.optimal_parameters.table_size * GCONFIG.counterSize,
                                              1, false);
                    }
                    else {
                        _cbf[inst->epochID] = new counting_bloom_filter(
                                                  std::move(h),
                                                  _parameters.optimal_parameters.table_size,
                                                  GCONFIG.counterSize, false);
                    }
                    _cbf[inst->epochID]->add(inst->instAddr());
                }
                break;
            case utils::IDEAL:
                warn_once("Ideal is checking counter saturation; added for rebuttal");
                if (IN_MAP(inst->epochID, _sb)) {
                    if (_sb.at(inst->epochID)[inst->instAddr()] >= _max_counter) {
                        SBCounterOverflows++;
                        _counterOverflowBuffer[inst->epochID][inst->instAddr()] += 1;
                    }
                }
                break;
            default:
                panic("Unknown SB structure!");
        }

        if (IN_MAP(inst->epochID, _sb)) {
            _sb.at(inst->epochID)[inst->instAddr()] += 1;
        } else {
            _sb[inst->epochID][inst->instAddr()] += 1;
        }
    }

    void retire(DynInstPtr inst) override {
        auto epochID = inst->epochID;
        switch (GCONFIG.sbHW) {
            case utils::BLOOM:
                return;
            case utils::COUNTING_BLOOM:
                if (IN_MAP(epochID, _cbf) &&
                    _cbf.at(epochID)->lookup(inst->instAddr()) > 0) {
                    _cbf.at(epochID)->remove(inst->instAddr());
                    SBRetireDeletions++;
                }
                break;
            case utils::IDEAL:
                if (IN_MAP(epochID, _sb) && IN_MAP(inst->instAddr(), _sb.at(epochID)) &&
                    IN_MAP(epochID, _counterOverflowBuffer) &&
                    IN_MAP(inst->instAddr(), _counterOverflowBuffer.at(epochID))) {
                    if (_sb.at(epochID).at(inst->instAddr()) ==
                        _counterOverflowBuffer.at(epochID).at(inst->instAddr())) {
                        _counterOverflowBuffer.at(epochID)[inst->instAddr()] -= 1;
                        if (_counterOverflowBuffer.at(epochID)[inst->instAddr()] == 0) {
                            _counterOverflowBuffer.at(epochID).erase(inst->instAddr());
                        }
                    }
                }
                SBRetireDeletions++;
                break;
            default:
                panic("Unknown SB structure!");
        }

        if (IN_MAP(epochID, _sb) && IN_MAP(inst->instAddr(), _sb.at(epochID))) {
            _sb.at(epochID)[inst->instAddr()] -= 1;
            if (_sb.at(epochID)[inst->instAddr()] == 0) {
                _sb.at(epochID).erase(inst->instAddr());
            }
        }
    }

    void squash(DynInstPtr inst) override { return; }

   private:
    size_t _max_active;
    long long _elems;
    size_t _max_counter;

    std::unordered_map<uint64_t, SquashBuffer> _sb;
    std::unordered_map<uint64_t, SquashBuffer> _counterOverflowBuffer;
    std::unordered_map<uint64_t, bloom_filter *> _bf;
    std::unordered_map<uint64_t, counting_bloom_filter *> _cbf;

    bloom_parameters _parameters;
    uint64_t _overflowed_epoch = 0;
    bool _ar_overflowed = false;

    Stats::Scalar SBRetireDeletions;
    Stats::Scalar SBCounterOverflows;
    Stats::Distribution activeRecords;
};

#endif
