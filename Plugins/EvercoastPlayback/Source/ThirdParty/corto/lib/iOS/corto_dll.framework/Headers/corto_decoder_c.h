#ifndef _CORTO_DECODER_C_H_
#define _CORTO_DECODER_C_H_

// This is strict C API that Unreal can use so that avoid so many weird STL related runtime headache.
// DO NOT USE decoder.h

#include <stdint.h>

// If compiling with Visual Studio
#if defined(_MSC_VER)
#define EXPORT_API __declspec(dllexport)
#else
// Other platforms don't need this
#define EXPORT_API __attribute__((__visibility__("default")))
#endif


extern "C"
{
    typedef struct _Corto_Color
    {
        float r;
        float g;
        float b;
        float a;
    } Corto_Color;

    typedef struct _Corto_Vector2
    {
        float x;
        float y;
    } Corto_Vector2;

    typedef struct _Corto_Vector3
    {
        float x;
        float y;
        float z;
    } Corto_Vector3;

    typedef struct _Corto_DecoderInfo
    {
        uint32_t nvert;
        uint32_t nface;
        uint32_t hasNormal;
    } Corto_DecoderInfo;


    void EXPORT_API *Corto_CreateDecoder(int length, const unsigned char* data, Corto_DecoderInfo* decoderInfo);
    void EXPORT_API Corto_DestroyDecoder(void* decoder_handler);
    int EXPORT_API Corto_DecodeMesh(void* decoder_handler, Corto_Vector3* vertices, uint32_t* indices, Corto_Vector3* normals, Corto_Color* colors, Corto_Vector2* texcoord);
}
#endif // _CORTO_DECODER_C_H_
