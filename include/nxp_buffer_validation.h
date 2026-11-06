#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Shared by the production C/C++ paths and the host regression tests. All
 * lengths describe actual backing storage; no device I/O is performed here. */
static inline int NxpValidateEepromBlob(const uint8_t* data,size_t length,uint8_t* count)
{
    size_t offset=0,records=0;
    int dlma=0;
    if(!count) return 0;
    *count=0;
    if((length && !data) || length>UINT16_MAX) return 0;
    while(offset<length) {
        size_t span;
        if(length-offset<3 || data[offset]!=0xa0 || !data[offset+2]) return 0;
        span=(size_t)data[offset+2]+3;
        /* One count byte must fit beside each record in a 255-byte command. */
        if(span>length-offset || span>254 || ++records>UINT8_MAX) return 0;
        if(data[offset+1]==0x34) {
            if(dlma || data[offset+2]!=210) return 0;
            dlma=1;
        }
        offset+=span;
    }
    *count=(uint8_t)records;
    return 1;
}

/* Output is a count-prefixed SET_CONFIG payload. The special DLMA parameter
 * is returned separately, preserving its documented 210-byte value schema. */
static inline int NxpSplitEepromBlob(const uint8_t* data,size_t length,
    uint8_t* output,size_t capacity,uint8_t dlma[214],size_t* written,int* hasDlma)
{
    uint8_t count;
    size_t offset=0,used=1;
    int found=0;
    if(!output || !written || !hasDlma || !dlma || !NxpValidateEepromBlob(data,length,&count) ||
       capacity<length+1) return 0;
    /* Validation precedes every output/state change. */
    memset(dlma,0,214);
    while(offset<length) {
        const size_t span=(size_t)data[offset+2]+3;
        if(data[offset+1]==0x34) {
            dlma[0]=1;memcpy(dlma+1,data+offset,span);found=1;--count;
        } else { memcpy(output+used,data+offset,span);used+=span; }
        offset+=span;
    }
    output[0]=count;*written=used;*hasDlma=found;
    return 1;
}

static inline int NxpValidateFirmwareImage(const uint8_t* data,size_t length)
{
    size_t offset=0;
    if(!data || length<=6 || length>UINT16_MAX) return 0;
    while(offset<length) {
        size_t span;
        if(length-offset<2) return 0;
        span=((size_t)data[offset]<<8)|data[offset+1];
        if(!span || span>length-offset-2) return 0;
        offset+=span+2;
    }
    return offset==length;
}

static inline size_t NxpDownloadPayloadLength(const uint8_t header[2])
{ return ((((size_t)header[0]<<8)|header[1])&0x1ff)+2; }

/* Build one complete SET_CONFIG payload without consuming caller state. */
static inline int NxpBuildRfBatch(const uint8_t* data,size_t length,size_t position,
    uint8_t remaining,uint8_t output[255],size_t* used,size_t* next,uint8_t* copied)
{
    size_t cursor=position,total=0,bytes=1;uint8_t count=0;
    if(!data || !output || !used || !next || !copied || position>length || !remaining) return 0;
    /* Validate the complete remainder before serializing a prefix. */
    while(cursor<length) {
        size_t span;
        if(length-cursor<3 || data[cursor]!=0xa0 || !data[cursor+2]) return 0;
        span=(size_t)data[cursor+2]+3;
        if(span>254 || span>length-cursor || ++total>remaining) return 0;
        cursor+=span;
    }
    if(total!=remaining) return 0;
    cursor=position;
    while(count<remaining && count<27) {
        size_t span=(size_t)data[cursor+2]+3;
        if(span>255-bytes) break;
        memcpy(output+bytes,data+cursor,span);bytes+=span;cursor+=span;++count;
    }
    if(!count) return 0;
    output[0]=count;*used=bytes;*next=cursor;*copied=count;return 1;
}

/* GetConfigRaw callback envelope is [payload length,status,count,TLVs],
 * held in an owned 255-byte array by CompleteGetConfigRaw. */
static inline int NxpFindConfigValue(const uint8_t* reply,uint8_t id,const uint8_t** value,size_t* length)
{
    size_t cursor=3,count=0,end;int found=0;
    if(!reply || !value || !length || reply[0]<2 || reply[0]>254 || reply[1]) return 0;
    end=(size_t)reply[0]+1;*value=NULL;*length=0;
    while(cursor<end) {
        size_t span;
        if(end-cursor<3 || reply[cursor]!=0xa0) return 0;
        span=(size_t)reply[cursor+2]+3;
        if(span>end-cursor) return 0;
        if(reply[cursor+1]==id) {
            if(found) return 0;
            found=1;*value=reply+cursor+3;*length=reply[cursor+2];
        }
        cursor+=span;++count;
    }
    return count==reply[2] && found;
}
