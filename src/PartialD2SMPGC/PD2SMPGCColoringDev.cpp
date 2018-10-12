/*************************************************************************
    > File Name: PD2SMPGCColoringDev.cpp
    > Author: xc
    > Descriptions: 
    > Created Time: Tue 28 Aug 2018 11:04:48 AM EDT
 ************************************************************************/
#include"PD2SMPGCColoring.h"
#include<iostream>
#include <bits/stdc++.h>
using namespace std;
using namespace ColPack;

#ifdef PD2_MASKWIDE_64
        #define PD2_MASKWIDE 64
#else
        #define PD2_MASKWIDE 32
#endif



// ============================================================================
// GM3P   is GM's algorithm 3 phase: Color, Detect and Conflicts
// VBBIT  is Vertice Based, Forbinden Array using Binary information, using EdegFiltering 
// ============================================================================
int PD2SMPGCColoring::PD2_OMP_GM3P_BIT(const int side, int nT, int &colors, vector<int>&vtxColor, const int local_order){
    if(nT<=0) { printf("Warning, number of threads changed from %d to 1\n",nT); nT=1; }
    omp_set_num_threads(nT);

    double tim_local_order =.0;
    double tim_color       =.0;                     // run time
    double tim_detect      =.0;                     // run time
    double tim_recolor     =.0;                     // run time
    double tim_maxc        =.0;
    double tim_total       =.0;                     // run time
    int    n_conflicts     = 0;                     // Number of conflicts 

    const int          N             = (side==PD2SMPGC::L)?(GetLeftVertexCount()):(GetRightVertexCount()); 
    const vector<int>& srcPtr        = (side==PD2SMPGC::L)?(GetLeftVertices()   ):(GetRightVertices()   );
    const vector<int>& dstPtr        = (side==PD2SMPGC::L)?(GetRightVertices()  ):(GetLeftVertices()    );
    const vector<int>& vtxVal        = GetEdges();
    const vector<int>& ordered_queue = global_ordered_vertex(side);
    //const int          srcMaxDegree  = (side==PD2SMPGC::L)?GetMaximumLeftVertexDegree():GetMaximumRightVertexDegree();
    //const int          dstMaxDegree  = (side==PD2SMPGC::L)?GetMaximumRightVertexDegree():GetMaximumLeftVertexDegree();
    //const int          BufSize       = min(N, dstMaxDegree * srcMaxDegree +1);
    colors=0;                       
    vtxColor.assign(N, -1);

    // pre-partition
    vector<vector<int>> QQ(nT);
    for(int tid=0; tid<nT; tid++) QQ[tid].reserve(N/nT+1+16); // +1 for even/odd, +16 for bus wide of 64bit machine
    {
        vector<int> lens(nT,N/nT), disp(nT+1,0);
        for(int tid=0; tid<N%nT; tid++) lens[tid]++;
        for(int tid=0; tid<nT; tid++) disp[tid+1]=disp[tid] + lens[tid];
        for(int tid=0; tid<nT; tid++)
            QQ[tid].insert(QQ[tid].end(), ordered_queue.begin()+disp[tid], ordered_queue.begin()+disp[tid+1]);
    }

    if(sizeof(unsigned long long int)!=8) printf("Warning!  sizeof(ForbidenArray) is not 64 bit wide.\n");

    tim_local_order =-omp_get_wtime();
    switch(local_order){
        case ORDER_NONE: break;
        case ORDER_NATURAL:
            #pragma omp parallel
            {
                const int tid = omp_get_thread_num();
                local_natural_ordering(QQ[tid]);
            }
            break;
        case ORDER_RANDOM:
            #pragma omp parallel
            {
                const int tid = omp_get_thread_num();
                local_random_ordering(QQ[tid]);
            }
            break;
        case ORDER_LARGEST_FIRST:
            #pragma omp parallel
            {
                const int tid = omp_get_thread_num();
                local_largest_degree_first_ordering(side, QQ[tid]);
            }
            break;
        default:
            printf("Error! PD2_OMP_GM3P tring to use order no. %d. But it is not supported.\n", local_order);
            exit(1);
    }
    tim_local_order +=omp_get_wtime();

    tim_color =- omp_get_wtime();
    // phase pseudo coloring
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        const vector<int>& Q = QQ[tid];

#ifdef PD2_MASKWIDE_64
        unsigned long long int Mask = ~0; 
#else
        unsigned int Mask = ~0;
#endif

        for(const auto v : Q){    //v-w-u
            int vc = -1;
            int offset_mask=0;
            while(vc==-1){ //for(int offset_mask=0; offset_mask!=NUM_ROUNDS_MASK; offset_mask++){
                Mask = ~0;
                const int LOW = (offset_mask++)*PD2_MASKWIDE;
                for(int iw=srcPtr[v]; iw!=srcPtr[v+1]; iw++){
                    const auto w = vtxVal[iw];
                    for(int iu=dstPtr[w]; iu!=dstPtr[w+1]; iu++){
                        if(v==vtxVal[iu]) continue;
                        const auto uc_local = vtxColor[ vtxVal[iu] ] - LOW;
                        if(uc_local >=0 && uc_local < PD2_MASKWIDE){
                            Mask &= ~(1<<(uc_local));
                        }
                    }// end for distance two neighbors
                }// end for distance one neighbors
                
                // find first settled bit
                for(int i=0; i<PD2_MASKWIDE; i++) {
                    if(Mask&(1<<i)){
                        vc=i;
                        vtxColor[v] = LOW+i;
                        break;
                    }
                }
            }// end while vc==-1
        }// end for v
    }// end omp parallel
    tim_color += omp_get_wtime();
    // phase conflicts detection
    tim_detect =- omp_get_wtime();
    n_conflicts=0;
    #pragma omp parallel reduction(+:n_conflicts)
    {
        const int tid = omp_get_thread_num();
        vector<int>& Q = QQ[tid];
        for(int iv=0; iv<(signed)Q.size(); iv++){
            bool  b_false_colored = false;
            const int v = Q[iv];
            const int vc = vtxColor[v];
            for(int iw=srcPtr[v]; b_false_colored==false && iw!=srcPtr[v+1]; iw++) {
                const int w = vtxVal[iw];
                for(int iu=dstPtr[w]; iu!=dstPtr[w+1]; iu++) {
                    const auto u = vtxVal[iu];
                    if(v <= u) continue;
                    if(vc== vtxColor[u]){
                        b_false_colored=true;
                        Q[n_conflicts++]=v;
                        vtxColor[v]=-1;
                        break;
                    }
                }
            }
        }
        Q.resize(n_conflicts);
    }// end omp parallel
    tim_detect  += omp_get_wtime();

    tim_recolor =- omp_get_wtime();
    // serial coloring remains part
    {
#ifdef PD2_MASKWIDE_64 
        unsigned long long int Mask = ~0;
#else
        unsigned int Mask = ~0;
#endif
        for(int tid=0; tid<nT; tid++){
            const vector<int>& Q = QQ[tid];
            for(const auto v : Q) {
                int vc=-1;
                int offset_mask=0;
                while(vc==-1){
                    Mask = ~0;
                    const int LOW = (offset_mask++)*PD2_MASKWIDE;
                    for(int iw=srcPtr[v]; iw!=srcPtr[v+1]; iw++) {
                        const auto w = vtxVal[iw];
                        for(int iu=dstPtr[w]; iu!=dstPtr[w+1]; iu++) {
                            if(v==vtxVal[iu]) continue;
                            const auto uc_local = vtxColor[vtxVal[iu]]-LOW;
                            if(uc_local>=0 && uc_local< PD2_MASKWIDE) 
                                Mask &= ~(1<<(uc_local));
                        }
                    }
                    //find first settled bit
                    for(int i=0; i<PD2_MASKWIDE; i++){
                        if(Mask&(1<<i)){
                            vc=i;
                            vtxColor[v]=LOW+i;
                            break;
                        }
                    }
                }// end while vc==-1
            }// end for v
        }// end for tid
    }
    tim_recolor += omp_get_wtime();

    // get number of colors
    tim_maxc = -omp_get_wtime();
    #pragma omp parallel for reduction(max:colors)
    for(int i=0; i<N; i++){
        colors = max(colors, vtxColor[i]);
    }
    colors++; //number of colors, 
    tim_maxc += omp_get_wtime();

    tim_total = tim_color+tim_detect+tim_recolor+tim_maxc+tim_local_order;

    string lotag = "";
    switch(local_order){
        case ORDER_NONE:    lotag="None"; break;
        case ORDER_NATURAL: lotag="NT"; break;
        case ORDER_RANDOM:  lotag="RD"; break;
        case ORDER_LARGEST_FIRST: lotag="LF"; break;
        default:
            printf("Error! PD2_OMP_GM3P tring to use local order %d. which is not supported.\n",local_order); 
            exit(1);
    }
    printf("@PD2GM3PBIT(%d)LO(%s)_side_nT_c_T_Tcolor_Tdetect_Trecolor_TmaxC_Tlo_nCnf\t",PD2_MASKWIDE,lotag.c_str());
    printf("%s ", (side==PD2SMPGC::L)?"L":"R");
    printf("%d\t",  nT);    
    printf("%d\t",  colors);    
    printf("%lf\t", tim_total);
    printf("%lf\t", tim_color);
    printf("%lf\t", tim_detect);
    printf("%lf\t", tim_recolor);
    printf("%lf\t", tim_maxc);
    printf("%lf\t", tim_local_order);
    printf("%d\t",  n_conflicts);
#ifdef PD2_VARIFY
    printf("%s", cnt_pd2conflict(side, vtxColor)?"Failed":"Varified");
#endif
    printf("\n");
    return _TRUE;

}



// ============================================================================
// GMMP   is GM's algorithm multiple phase: Color, Detect ....
// VBBIT  is Vertice Based, Forbinden Array using Binary information, using EdegFiltering 
// ============================================================================
int PD2SMPGCColoring::PD2_OMP_GMMP_BIT(const int side, int nT, int &colors, vector<int>&vtxColor, const int local_order){
    if(nT<=0) { printf("Warning, number of threads changed from %d to 1\n",nT); nT=1; }
    omp_set_num_threads(nT);

    double tim_local_order =.0;
    double tim_color       =.0;                     // run time
    double tim_detect      =.0;                     // run time
    double tim_maxc        =.0;
    double tim_total       =.0;                     // run time
    int    n_conflicts     = 0;                     // Number of conflicts 
    int    n_loops         = 0;

    const int          N             = (side==PD2SMPGC::L)?(GetLeftVertexCount()):(GetRightVertexCount()); 
    const vector<int>& srcPtr        = (side==PD2SMPGC::L)?(GetLeftVertices()   ):(GetRightVertices()   );
    const vector<int>& dstPtr        = (side==PD2SMPGC::L)?(GetRightVertices()  ):(GetLeftVertices()    );
    const vector<int>& vtxVal        = GetEdges();
    const vector<int>& ordered_queue = global_ordered_vertex(side);
    //const int          srcMaxDegree  = (side==PD2SMPGC::L)?GetMaximumLeftVertexDegree():GetMaximumRightVertexDegree();
    //const int          dstMaxDegree  = (side==PD2SMPGC::L)?GetMaximumRightVertexDegree():GetMaximumLeftVertexDegree();
    //const int          BufSize       = min(N, dstMaxDegree * srcMaxDegree +1);
    colors=0;                       
    vtxColor.assign(N, -1);

    // pre-partition
    vector<vector<int>> QQ(nT);
    for(int tid=0; tid<nT; tid++) QQ[tid].reserve(N/nT+1+16); // +1 for even/odd, +16 for bus wide of 64bit machine
    {
        vector<int> lens(nT,N/nT), disp(nT+1,0);
        for(int tid=0; tid<N%nT; tid++) lens[tid]++;
        for(int tid=0; tid<nT; tid++) disp[tid+1]=disp[tid] + lens[tid];
        for(int tid=0; tid<nT; tid++)
            QQ[tid].insert(QQ[tid].end(), ordered_queue.begin()+disp[tid], ordered_queue.begin()+disp[tid+1]);
    }

    if(sizeof(unsigned long long int)!=8) printf("Warning!  sizeof(ForbidenArray) is not 64 bit wide.\n");
    int num_uncolored_vertex = N;
    while(num_uncolored_vertex!=0) {
        tim_local_order -=omp_get_wtime();
        switch(local_order){
            case ORDER_NONE: break;
            case ORDER_NATURAL:
                #pragma omp parallel
                {
                    const int tid = omp_get_thread_num();
                    local_natural_ordering(QQ[tid]);
                }
                break;
            case ORDER_RANDOM:
                #pragma omp parallel
                {
                    const int tid = omp_get_thread_num();
                    local_random_ordering(QQ[tid]);
                }
                break;
            case ORDER_LARGEST_FIRST:
                #pragma omp parallel
                {
                    const int tid = omp_get_thread_num();
                    local_largest_degree_first_ordering(side, QQ[tid]);
                }
                break;
            default:
                printf("Error! PD2_OMP_GM3P tring to use order no. %d. But it is not supported.\n", local_order);
                exit(1);
        }
        tim_local_order +=omp_get_wtime();
        
        tim_color -= omp_get_wtime();
        // phase pseudo coloring
        #pragma omp parallel
        {
            const int tid = omp_get_thread_num();
            const vector<int>& Q = QQ[tid];
#ifdef PD2_MASKWIDE_64
            unsigned long long int Mask = ~0;
#else
            unsigned int Mask = ~0;
#endif
            for(const auto v : Q){    //v-w-u
                int vc = -1;
                int offset_mask=0;
                while(vc==-1){ 
                    Mask=~0;
                    const int LOW = (offset_mask++)*PD2_MASKWIDE;
                    for(int iw=srcPtr[v]; iw!=srcPtr[v+1]; iw++){
                        const auto w = vtxVal[iw];
                        for(int iu=dstPtr[w]; iu!=dstPtr[w+1]; iu++){
                            if(v==vtxVal[iu]) continue;
                            const int uc_local = vtxColor[ vtxVal[iu] ]-LOW;
                            if(uc_local>=0 && uc_local < PD2_MASKWIDE)
                                Mask &= ~(1<<uc_local);
                        }// end for distance two neighbors
                    }// end for distance one neighbors
                    
                    //find first setted bit
                    for(int i=0; i<PD2_MASKWIDE; i++){
                        if(Mask&(1<<i)){
                            vc=i;
                            vtxColor[v]=LOW+i;
                            break;
                        }
                    }
                }// end while vc==-1
            }// end for v
        }// end omp parallel
        tim_color += omp_get_wtime();
        
        // phase conflicts detection
        tim_detect -= omp_get_wtime();
        num_uncolored_vertex = 0;
        #pragma omp parallel reduction(+:num_uncolored_vertex)
        {
            const int tid = omp_get_thread_num();
            vector<int>& Q = QQ[tid];
            for(int iv=0; iv<(signed)Q.size(); iv++){
                bool  b_visfalse_colored = false;
                const int v = Q[iv];
                const int vc = vtxColor[v];
                for(int iw=srcPtr[v]; b_visfalse_colored==false && iw!=srcPtr[v+1]; iw++) {
                    const int w = vtxVal[iw];
                    for(int iu=dstPtr[w]; iu!=dstPtr[w+1]; iu++) {
                        const auto u = vtxVal[iu];
                        if(v <= u) continue; 
                        if(vc== vtxColor[u]){
                            b_visfalse_colored=true;
                            Q[num_uncolored_vertex++]=v;
                            vtxColor[v]=-1;
                            break;
                        }
                    }// end for d2 neighbors
                }// end for d1 neighbors
            }// end for v
            Q.resize(num_uncolored_vertex);
        }// end omp parallel
        tim_detect  += omp_get_wtime();

        n_conflicts += num_uncolored_vertex;
        n_loops++;
    }// end while
    
    // get number of colors
    tim_maxc = -omp_get_wtime();
    #pragma omp parallel for reduction(max:colors)
    for(int i=0; i<N; i++){
        colors = max(colors, vtxColor[i]);
    }
    colors++; //number of colors, 
    tim_maxc += omp_get_wtime();

    tim_total = tim_color+tim_detect+tim_maxc+tim_local_order;

    string lotag = "";
    switch(local_order){
        case ORDER_NONE:    lotag="None"; break;
        case ORDER_NATURAL: lotag="NT"; break;
        case ORDER_RANDOM:  lotag="RD"; break;
        case ORDER_LARGEST_FIRST: lotag="LF"; break;
        default:
            printf("Error! PD2_OMP_GM3P tring to use local order %d. which is not supported.\n",local_order); 
            exit(1);
    }
    
    printf("@PD2GMMPBIT(%d)LO(%s)_side_nT_c_T_Tcolor_Tdetect_Trecolor_TmaxC_Tlo_nCnf\t",PD2_MASKWIDE, lotag.c_str());
    printf("%s ", (side==PD2SMPGC::L)?"L":"R");
    printf("%d\t",  nT);    
    printf("%d\t",  colors);    
    printf("%lf\t", tim_total);
    printf("%lf\t", tim_color);
    printf("%lf\t", tim_detect);
    printf("%lf\t", tim_maxc);
    printf("%lf\t", tim_local_order);
    printf("%d\t",  n_conflicts);
    printf("%d\t", n_loops);
#ifdef PD2_VARIFY
    printf("%s", cnt_pd2conflict(side, vtxColor)?"Failed":"Varified");
#endif
    printf("\n");
    return _TRUE;

}




