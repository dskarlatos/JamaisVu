#ifndef GLOBAL_UTILS_HH
#define GLOBAL_UTILS_HH

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "debug/Tracer.hh"

#define IN_SET(ELEM, SET) (SET.find(ELEM) != SET.end())
#define IN_MAP(KEY, MAP) (MAP.find(KEY) != MAP.end())

#define IN_SETPTR(ELEM, SET) (SET->find(ELEM) != SET->end())
#define IN_MAPPTR(KEY, MAP) (MAP->find(KEY) != MAP->end())

#define DICT_GET(ELEM, DICT, DEFT) (IN_MAP(ELEM, DICT) ? DICT[pc] : DEFT)
#define DICTPTR_GET(ELEM, DICT, DEFT) (IN_MAPPTR(ELEM, DICT) ? (*DICT)[pc] : DEFT)

#define CHECK_SEQNUM(TID) (((bridge::GCONFIG.hasLowerBound && bridge::GCONFIG.youngestSeqNums[TID] >= bridge::GCONFIG.lowerSeqNum) || !bridge::GCONFIG.hasLowerBound) && \
                           ((bridge::GCONFIG.hasUpperBound && bridge::GCONFIG.youngestSeqNums[TID] <= bridge::GCONFIG.upperSeqNum) || !bridge::GCONFIG.hasUpperBound))

#define DSTATE(STATE, INST)                                                                \
    do {                                                                                   \
        if (CHECK_SEQNUM(INST->threadNumber)) {                                            \
            DPRINTF(Tracer, "%#x+%lli(%c)@%lli(e+%lli): [%s]\n", INST->instAddr(),         \
                    INST->microPC(), INST->typeCode, INST->seqNum, INST->epochID, #STATE); \
        }                                                                                  \
    } while (0)

#define CSPRINT(STATE, INST, x, ...)                                                                    \
    do {                                                                                                \
        if (CHECK_SEQNUM(INST->threadNumber)) {                                                         \
            DPRINTF(Tracer, "%#x+%lli(%c)@%lli(e+%lli): [%s]: " x, INST->instAddr(),                    \
                    INST->microPC(), INST->typeCode, INST->seqNum, INST->epochID, #STATE, __VA_ARGS__); \
        }                                                                                               \
    } while (0)

#define CCSPRINT(FLAG, STATE, INST, x, ...)                                                             \
    do {                                                                                                \
        if (CHECK_SEQNUM(INST->threadNumber)) {                                                         \
            DPRINTF(FLAG, "%#x+%lli(%c)@%lli(e+%lli): [%s]: " x, INST->instAddr(),                      \
                    INST->microPC(), INST->typeCode, INST->seqNum, INST->epochID, #STATE, __VA_ARGS__); \
        }                                                                                               \
    } while (0)

#define CPRINT(x, TID, ...)          \
    do {                             \
        if (CHECK_SEQNUM(TID)) {     \
            DPRINTF(x, __VA_ARGS__); \
        }                            \
    } while (0)

#define MEMDBG(STATE, INST, ADDR) DPRINTF(MemDbg, "%#x+%lli(%c)@%lli: [%s]: %#x\n", INST->instAddr(), \
                                          INST->microPC(), INST->typeCode, INST->seqNum, #STATE, ADDR);

#define MRAPRINT(STATE, INST, STATICINST)                                                 \
    do {                                                                                  \
        if (CHECK_SEQNUM(INST->threadNumber)) {                                           \
            DPRINTF(Tracer, "%#x+%lli(%c)@%lli(e+%lli): [%s]: %d\n",                      \
                    INST->instAddr(),                                                     \
                    INST->microPC(), INST->typeCode, INST->seqNum, INST->epochID, #STATE, \
                    STATICINST->numReplays());                                            \
        }                                                                                 \
    } while (0)

#define TICKS_PER_CYCLE 500  //hack for now update based on Frequency

namespace utils {
typedef enum {
    UNSAFE,    // no protection at all
    FENCE,     // fence loads only
    FENCE_ALL  // fence every instruction
} HWType;

typedef enum {
    NO_DETECT,  // no replay detection
    COUNTER,    // use a counter
    BUFFER,     // use a buffer
    EPOCH,
} replayDetection;

typedef enum {
    ISSUE,  // threat is issue
    EXEC,   // threat is execute
} replayDetectionThreat;

typedef enum {
    INVALID = 0,
    ITERATION = 1,
    LOOP = 2,
    ROUTINE = 3,
} EpochScale;

typedef enum {  //SB Structure
    IDEAL,
    BLOOM,
    COUNTING_BLOOM,
} sbStruct;

struct CustomConfigs {
    std::string HWName;
    std::string threatModel;
    std::string replayDetScheme;
    std::string replayDetThreat;
    std::string sbHWStruct;
    uint64_t maxInsts;
    uint32_t maxReplays;
    size_t maxSBSize;

    bool isSpectre;
    bool isFuturistic;

    bool liftOnClear;
    long long projectedElemCnt;

    std::string epochInfoPath;
    EpochScale epochSize;
    bool deleteOnRetire;
    size_t activeRecords;
    bool checkAllRecords;
    size_t counterSize;

    HWType hw;
    replayDetection replayDet;
    replayDetectionThreat replayThreat;
    sbStruct sbHW;

    // Counter Cache
    size_t CCAssoc, CCSets;
    uint64_t CCMissLatency;
    bool CCIdeal, CCEnable;

    // debugging related
    uint64_t lowerSeqNum, upperSeqNum;  // range for print DSTATE information
    bool hasLowerBound, hasUpperBound;
    uint64_t youngestSeqNums[65536];
};

typedef std::unordered_set<uint64_t> Counter_t;
typedef std::shared_ptr<Counter_t> Counter_p;
typedef std::unordered_map<uint64_t, Counter_p> CounterMap_t;
typedef std::shared_ptr<CounterMap_t> CounterMap_p;
typedef std::list<uint64_t> LRUStatus;
typedef std::shared_ptr<LRUStatus> LRUStatus_p;

struct CounterCache {  // an Counter cache with LRU replacement policy
    CounterCache() = default;

    CounterCache(size_t numWays, size_t numSets, CounterMap_p CounterMap, uint64_t missLatency, bool ideal = false) : numWays(numWays), numSets(numSets), CounterMap(CounterMap), missLatency(missLatency), ideal(ideal) {
        // miss penalty is in cycles
        for (auto i = 0; i < numSets; i++) {
            LRUStatus_p lru(new LRUStatus);
            CounterMap_p counterc(new CounterMap_t);

            LRUs.emplace_back(lru);
            cache.emplace_back(counterc);

            // setup counters
            hitCountPerSet.emplace_back(0);
            reqCountPerSet.emplace_back(0);
            replaceCountPerSet.emplace_back(0);
        }
    }

    Counter_p refer(uint64_t pc, uint64_t curTick, bool &hit) {
        pc = pc / 64;
        size_t index = pc % numSets;

        reqCountPerSet[index]++;
        if (ideal) {
            hit = true;
            hitCountPerSet[index]++;
            return DICTPTR_GET(pc, CounterMap, nullptr);
        }

        if (IN_MAPPTR(pc, cache[index])) {
            auto loadTick = issueTime[pc];
            if (curTick - loadTick >= TICKS_PER_CYCLE * missLatency) {
                // update LRU
                LRUs[index]->remove(pc);
                LRUs[index]->push_front(pc);

                hit = true;
                hitCountPerSet[index]++;
                return (*cache[index])[pc];
            } else {
                // data has not arrived
                hit = false;
                return nullptr;
            }
        }
        return nullptr;
    }

    uint64_t fetch(uint64_t pc, uint64_t curTick) {
        pc = pc / 64;
        // cache miss
        size_t index = pc % numSets;
        if (!IN_MAPPTR(pc, cache[index])) {
            loadCounter(pc, curTick);
        }
        return TICKS_PER_CYCLE * missLatency + curTick;
    }

    size_t getWay() const { return numWays; }
    size_t getSet() const { return numSets; }
    double hitRate() const {
        double ref_cnt = 0, hit_cnt = 0;
        for (auto c : hitCountPerSet) {
            hit_cnt += c;
        }
        for (auto c : reqCountPerSet) {
            ref_cnt += c;
        }
        if (ref_cnt == 0)
            return 0.0;
        else
            return hit_cnt / ref_cnt;
    }

    uint64_t hitCnt() const {
        uint64_t cnt = 0;
        for (auto c : hitCountPerSet) {
            cnt += c;
        }
        return cnt;
    }

    uint64_t refCnt() const {
        uint64_t cnt = 0;
        for (auto c : reqCountPerSet) {
            cnt += c;
        }
        return cnt;
    }

   private:
    size_t numWays, numSets;
    CounterMap_p CounterMap;
    uint64_t missLatency;
    bool ideal;

    std::vector<LRUStatus_p> LRUs;
    std::vector<CounterMap_p> cache;
    std::unordered_map<uint64_t, uint64_t> issueTime;

    // statistics
    std::vector<uint64_t> hitCountPerSet;
    std::vector<uint64_t> reqCountPerSet;
    std::vector<uint64_t> replaceCountPerSet;

    void loadCounter(uint64_t line, uint64_t curTick) {
        auto pc = line;
        auto index = pc % numSets;

        assert(cache[index]->size() <= numWays);
        assert(LRUs[index]->size() <= numWays);

        if (cache[index]->size() == numWays) {
            evict(pc);
        }

        // load the data
        (*cache[index])[pc] = DICTPTR_GET(pc, CounterMap, nullptr);
        issueTime[pc] = curTick;

        // update LRU
        LRUs[index]->push_front(pc);

        assert(cache[index]->size() <= numWays);
        assert(LRUs[index]->size() <= numWays);
    }

    void evict(uint64_t line) {
        auto pc = line;
        auto index = pc % numSets;
        auto pc2evict = LRUs[index]->back();

        LRUs[index]->pop_back();
        cache[index]->erase(pc2evict);
        replaceCountPerSet[index]++;
    }
};

typedef std::shared_ptr<utils::CounterCache> CounterCache_p;

}  // namespace utils

namespace bridge {
extern utils::CustomConfigs GCONFIG;
extern std::vector<std::shared_ptr<utils::CounterCache>> CounterCaches;
}  // namespace bridge

#endif
