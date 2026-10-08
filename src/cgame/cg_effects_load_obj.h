#pragma once
#include <universal/ptr32.h>
#include <database/db_registry.h>
#include <gfx_d3d/fxprimitives.h>

struct FxImpactEntry // sizeof=0x8C
{
    Ptr32<const FxEffectDef> nonflesh[31];
    Ptr32<const FxEffectDef> flesh[4];
};

struct FxImpactTable // sizeof=0x8
{                                       // XREF: XAssetPoolEntry<FxImpactTable>/r
    Ptr32<const char> name;
    Ptr32<FxImpactEntry> table;
};

struct EffectFile // sizeof=0xB7C
{                                       // XREF: CG_RegisterImpactEffects_LoadObj/r
    const char *nonflesh[21][31];       // XREF: CG_RegisterImpactEffects_LoadObj+121/r
    const char *flesh[21][4];           // XREF: CG_RegisterImpactEffects_LoadObj+14E/o
};

FxImpactTable *__cdecl CG_RegisterImpactEffects(const char *mapname);
FxImpactTable *__cdecl CG_RegisterImpactEffects_LoadObj(const char *mapname);
int __cdecl CG_RegisterImpactEffects_NonFlesh(
                const char **szEffectFile,
                const char *pszTypeName,
                const FxEffectDef **fx);
int __cdecl CG_RegisterImpactEffects_Flesh(
                const char **szEffectFile,
                const char *pszTypeName,
                const FxEffectDef **fx,
                char *defaultEffectName);
const char *__cdecl CG_FleshTypeToName(unsigned int fleshTypeId);
void __cdecl CG_RegisterImpactEffectsForDir(char *dir, EffectFile *effectFile, char *listbuf);
char *__cdecl CG_ParseImpactEffects(
                const char *filename,
                const char *buf,
                int iTypeCount,
                const char *const *pszTypeName,
                EffectFile *effectFile);
int __cdecl compare_impact_files(const char **pe0, const char **pe1);
FxImpactTable *__cdecl CG_RegisterImpactEffects_FastFile();
