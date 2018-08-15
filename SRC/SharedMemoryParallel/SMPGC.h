/*************************************************************************
    > File Name: SMPGCDefine.h
    > Author: xc
    > Descriptions: 
    > Created Time: Fri 25 May 2018 04:34:23 PM EDT
 ************************************************************************/

#ifndef SMPGCDEFINE_H
#define SMPGCDEFINE_H
#include <string>
// ============================================================================
// SMPGC: Shared Memory Parallel Graph Coloring
// ----------------------------------------------------------------------------
// **OVERVIEW**
//
// SMPGCCore:                        Graph data, IO
//     |->SMPGCOrdering:             Ordering
//          |-> SMPGCColoring:       D1 Coloring
//          |-> D2SMPGCColoring:     D2 Coloring
//
// ----------------------------------------------------------------------------
// **LIST OF ALGORITHMS**
// * GM's Algorithm: Gebremedhin and Manne[1].
// * IP's Algorithm: Catalyurek Feo Gebremedhin and Halappanavar[2]
// * JP's Algorithm: Jones and Plassmann[3]
// * Luby's Alg
// ...
// ...
// ----------------------------------------------------------------------------
// **LIST OF PAPERS**
// [1] Scalable Parallel Graph Coloring Algorithms
// [2] Grah coloring algorithms for multi-core and massively multithreaded architectures
// [3] A Parallel Graph Coloring Heuristic
// ...
// ...
// ============================================================================

class SMPGC{
//public: // in comman Computer is LP64 Model. change the following in case not. 
    //typedef unsigned long long int uint64;
    //typedef          long long int  int64;
    //typedef unsigned int           uint32;
    //typedef          int            int32;
    //#ifndef INT32
    //    typedef uint64 UINT;
    //    typedef int64  INT ;
    //#else
    //    typedef uint32 UINT;
    //    typedef int32  INT;
    //#endif

public:
    static const int RAND_SEED           = 5489u;

    static const int HASH_SEED           = 5489u;
    static const int HASH_SHIFT          = 0XC2A50F;
    static const int HASH_NUM_HASH       = 4;

    static const std::string FORMAT_MM   ="MM"      ;
    static const std::string FORMAT_BINARY="BINARY"    ;

    static const int ORDER_NONE          = 0;
    static const int ORDER_NATURAL       = 1;
    static const int ORDER_RANDOM        = 2;
    static const int ORDER_LARGEST_FIRST = 3;
    static const int ORDER_SMALLEST_LAST = 4;

    static const int HYBRID_GM3P         = 1;
    static const int HYBRID_GMMP         = 2;
    static const int HYBRID_SERIAL       = 3;
    static const int HYBRID_STREAM       = 4;


public:
    SMPGC(){};
    ~SMPGC(){};
public:
    SMPGC(SMPGC&&)=delete;
    SMPGC(const SMPGC&)=delete;
    SMPGC& operator=(SMPGC&&)=delete;
    SMPGC& operator=(const SMPGC&)=delete;
};



#endif

