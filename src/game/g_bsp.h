#pragma once
#include <universal/ptr32.h>

#include "pathnode.h"

struct PathData // sizeof=0x28
{                                       // XREF: GameWorldSp/r
    unsigned int nodeCount;             // XREF: Bot_UpdatePath+47B/r
                                        // Bot_UpdatePath+484/r
    Ptr32<pathnode_t> nodes;                  // XREF: Bot_UpdatePath+49E/r
                                        // Bot_UpdateDirection+98D/r
    Ptr32<pathbasenode_t> basenodes;
    unsigned int chainNodeCount;
    Ptr32<unsigned __int16> chainNodeForNode;
    Ptr32<unsigned __int16> nodeForChainNode;
    int visBytes;
    Ptr32<unsigned __int8> pathVis;
    int nodeTreeCount;
    Ptr32<pathnode_tree_t> nodeTree;

    PathData()
    {
        memset(this, 0, sizeof(PathData)); // lwss add
    }
};

struct GameWorldSp // sizeof=0x2C
{
    Ptr32<const char> name;
    PathData path;
};

struct GameWorldMp // sizeof=0x2C
{                                       // XREF: .data:GameWorldMp gameWorldMp/r
    Ptr32<const char> name;
    PathData path;                      // XREF: Bot_UpdatePath+47B/r
};


#ifdef KISAK_SP
extern GameWorldSp gameWorldSp;
#else
extern GameWorldMp gameWorldMp;
#endif

extern GameWorldMp *gameWorldCurrent;
