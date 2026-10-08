#include "UserInfo.h"

PublicProfileInfo::PublicProfileInfo() : bdProfileInfo()
{
    //bdProfileInfo::bdProfileInfo(this);
    //this->__vftable = (PublicProfileInfo_vtbl *)&PublicProfileInfo::`vftable';
    //return this;
}

void PublicProfileInfo::serialize(bdByteBuffer *buffer)
{
}

bool PublicProfileInfo::deserialize(bdReference<bdByteBuffer> buffer)
{
    return false;
}

unsigned int PublicProfileInfo::sizeOf()
{
    return 280;
}

PrivateProfileInfo::PrivateProfileInfo() : bdProfileInfo()
{
    //bdProfileInfo::bdProfileInfo(this);
    //this->__vftable = (PrivateProfileInfo_vtbl *)&PrivateProfileInfo::`vftable';
    //return this;
}

void __thiscall PrivateProfileInfo::serialize(bdByteBuffer *buffer)
{
}

bool __thiscall PrivateProfileInfo::deserialize(bdReference<bdByteBuffer> buffer)
{
    return false;
}

unsigned int __thiscall PrivateProfileInfo::sizeOf()
{
    return 608;
}

