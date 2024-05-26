// Copyright (C) 2019 Evercoast Inc. All Rights Reserved.

#ifndef GHOST_TREE_PUBLIC_INCLUDE_EC_CODECS_DECODING_API_MACROS_HPP_
#define GHOST_TREE_PUBLIC_INCLUDE_EC_CODECS_DECODING_API_MACROS_HPP_

#include <memory>

#if defined(_MSC_VER)
#define EC_DECODING_API_EXPORT __declspec( dllexport )
#elif defined(EMSCRIPTEN)
#include <emscripten.h>
#define EC_DECODING_API_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EC_DECODING_API_EXPORT __attribute__((visibility ("default")))
#endif
//todo: experemental feature usage. Must put time into handle management.
namespace ec::codecs
{
typedef std::shared_ptr<void> HANDLE;
}
#endif  // GHOST_TREE_PUBLIC_INCLUDE_EC_CODECS_DECODING_API_MACROS_HPP_
