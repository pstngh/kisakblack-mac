#pragma once
#include <universal/ptr32.h>
#include <cstddef>

enum XAssetType : __int32
{                                                                             // XREF: XAsset/r
    ASSET_TYPE_XMODELPIECES = 0x0,
    ASSET_TYPE_PHYSPRESET = 0x1,
    ASSET_TYPE_PHYSCONSTRAINTS = 0x2,
    ASSET_TYPE_DESTRUCTIBLEDEF = 0x3,
    ASSET_TYPE_XANIMPARTS = 0x4,
    ASSET_TYPE_XMODEL = 0x5,
    ASSET_TYPE_MATERIAL = 0x6,
    ASSET_TYPE_TECHNIQUE_SET = 0x7,
    ASSET_TYPE_IMAGE = 0x8,
    ASSET_TYPE_SOUND = 0x9,
    ASSET_TYPE_SOUND_PATCH = 0xA,
    ASSET_TYPE_CLIPMAP = 0xB,
    ASSET_TYPE_CLIPMAP_PVS = 0xC,
    ASSET_TYPE_COMWORLD = 0xD,
    ASSET_TYPE_GAMEWORLD_SP = 0xE,
    ASSET_TYPE_GAMEWORLD_MP = 0xF,
    ASSET_TYPE_MAP_ENTS = 0x10,
    ASSET_TYPE_GFXWORLD = 0x11,
    ASSET_TYPE_LIGHT_DEF = 0x12,
    ASSET_TYPE_UI_MAP = 0x13,
    ASSET_TYPE_FONT = 0x14,
    ASSET_TYPE_MENULIST = 0x15,
    ASSET_TYPE_MENU = 0x16,
    ASSET_TYPE_LOCALIZE_ENTRY = 0x17,
    ASSET_TYPE_WEAPON = 0x18,
    ASSET_TYPE_WEAPONDEF = 0x19,
    ASSET_TYPE_WEAPON_VARIANT = 0x1A,
    ASSET_TYPE_SNDDRIVER_GLOBALS = 0x1B,
    ASSET_TYPE_FX = 0x1C,
    ASSET_TYPE_IMPACT_FX = 0x1D,
    ASSET_TYPE_AITYPE = 0x1E,
    ASSET_TYPE_MPTYPE = 0x1F,
    ASSET_TYPE_MPBODY = 0x20,
    ASSET_TYPE_MPHEAD = 0x21,
    ASSET_TYPE_CHARACTER = 0x22,
    ASSET_TYPE_XMODELALIAS = 0x23,
    ASSET_TYPE_RAWFILE = 0x24,
    ASSET_TYPE_STRINGTABLE = 0x25,
    ASSET_TYPE_PACK_INDEX = 0x26,
    ASSET_TYPE_XGLOBALS = 0x27,
    ASSET_TYPE_DDL = 0x28,
    ASSET_TYPE_GLASSES = 0x29,
    ASSET_TYPE_EMBLEMSET = 0x2A,
    ASSET_TYPE_COUNT = 0x2B,
    ASSET_TYPE_STRING = 0x2B,
    ASSET_TYPE_ASSETLIST = 0x2C,
};
inline XAssetType &operator++(XAssetType &t)
{
    t = static_cast<XAssetType>((static_cast<int>(t) + 1));
    return t;
}
inline XAssetType operator++(XAssetType &t, int)
{
    XAssetType old = t;
    t = static_cast<XAssetType>((static_cast<int>(t) + 1));
    return old;
}


union XAssetHeader // sizeof=0x4
{                                                                             // XREF: CG_AddVisionSetMenuItem+13/r
    Ptr32<struct XModelPieces> xmodelPieces;
    Ptr32<struct PhysPreset> physPreset;
    Ptr32<struct PhysConstraints> physConstraints;
    Ptr32<struct DestructibleDef> destructibleDef;
    Ptr32<struct XAnimParts> parts;
    Ptr32<struct XModel> model;
    Ptr32<struct Material> material;
    Ptr32<struct MaterialPixelShader> pixelShader;
    Ptr32<struct MaterialVertexShader> vertexShader;
    Ptr32<struct MaterialTechniqueSet> techniqueSet;
    Ptr32<struct GfxImage> image;
    Ptr32<struct SndBank> sound;
    Ptr32<struct SndPatch> soundPatch;
    Ptr32<struct clipMap_t> clipMap;
    Ptr32<struct ComWorld> comWorld;
    Ptr32<struct GameWorldSp> gameWorldSp;
    Ptr32<struct GameWorldMp> gameWorldMp;
    Ptr32<struct MapEnts> mapEnts;
    Ptr32<struct GfxWorld> gfxWorld;
    Ptr32<struct GfxLightDef> lightDef;
    Ptr32<struct Font_s> font;
    Ptr32<struct MenuList> menuList;
    Ptr32<struct menuDef_t> menu;
    Ptr32<struct LocalizeEntry> localize;
    Ptr32<struct WeaponVariantDef> weapon;
    Ptr32<struct SndDriverGlobals> sndDriverGlobals;
    Ptr32<struct FxEffectDef> fx;
    Ptr32<struct FxImpactTable> impactFx;
    Ptr32<struct RawFile> rawfile;
    Ptr32<struct StringTable> stringTable;
    Ptr32<struct PackIndex> packIndex;
    Ptr32<struct XGlobals> xGlobals;
    Ptr32<struct ddlRoot_t> ddlRoot;
    Ptr32<struct Glasses> glasses;
    Ptr32<struct TextureList> textureList;
    Ptr32<struct EmblemSet> emblemSet;
    Ptr32<void> data;

    XAssetHeader()
    {
        data = nullptr;
    }

    XAssetHeader(void *p)
    {
        data = p;
    }
};