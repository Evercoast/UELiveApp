// Copyright (C) 2019 Evercoast Inc. All Rights Reserved.

#ifndef GHOST_TREE_PUBLIC_INCLUDE_EC_CODECS_STREAMING_READER_API_MACROS_HPP_
#define GHOST_TREE_PUBLIC_INCLUDE_EC_CODECS_STREAMING_READER_API_MACROS_HPP_

#if defined(_MSC_VER)
#define EC_READING_API_EXPORT __declspec(dllexport)
#elif defined(EMSCRIPTEN)
#include <emscripten.h>
#define EC_READING_API_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EC_READING_API_EXPORT __attribute__((visibility("default")))
#endif

#endif // GHOST_TREE_PUBLIC_INCLUDE_EC_CODECS_STREAMING_READER_API_MACROS_HPP_
