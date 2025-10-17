#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "splat-types.h"


#if defined(_WIN32) && defined(LIBSPZ_DLL)
#ifdef LIBSPZ_EXPORTS
#define LIBSPZ_API __declspec(dllexport)
#else
#define LIBSPZ_API __declspec(dllimport)
#endif
#else
#define LIBSPZ_API
#endif

namespace spz {

struct PackedGaussiansHeader {
    uint32_t magic = 0x5053474e;  // NGSP = Niantic gaussian splat
    uint32_t version = 2;
    uint32_t numPoints = 0;
    uint8_t shDegree = 0;
    uint8_t fractionalBits = 0;
    uint8_t flags = 0;
    uint8_t reserved = 0;
};

// Represents a single inflated gaussian. Each gaussian has 236 bytes. Although the data is easier
// to intepret in this format, it is not more precise than the packed format, since it was inflated.
struct UnpackedGaussian {
  std::array<float, 3> position;  // x, y, z
  std::array<float, 4> rotation;  // x, y, z, w
  std::array<float, 3> scale;     // std::log(scale)
  std::array<float, 3> color;     // rgb sh0 encoding
  float alpha;                    // inverse logistic
  std::array<float, 15> shR;
  std::array<float, 15> shG;
  std::array<float, 15> shB;
};

// Represents a single low precision gaussian. Each gaussian has exactly 64 bytes, even if it does
// not have full spherical harmonics.
struct PackedGaussian {
  std::array<uint8_t, 9> position{};
  std::array<uint8_t, 3> rotation{};
  std::array<uint8_t, 3> scale{};
  std::array<uint8_t, 3> color{};
  uint8_t alpha = 0;
  std::array<uint8_t, 15> shR{};
  std::array<uint8_t, 15> shG{};
  std::array<uint8_t, 15> shB{};

  UnpackedGaussian unpack(
    bool usesFloat16, int32_t fractionalBits, const CoordinateConverter &c) const;
};

// Represents a full splat with lower precision. Each splat has at most 64 bytes, although splats
// with fewer spherical harmonics degrees will have less. The data is stored non-interleaved.
struct PackedGaussians {
  int32_t numPoints = 0;       // Total number of points (gaussians)
  int32_t shDegree = 0;        // Degree of spherical harmonics
  int32_t fractionalBits = 0;  // Number of bits used for fractional part of fixed-point coords
  bool antialiased = false;    // Whether gaussians should be rendered with mip-splat antialiasing

  std::vector<uint8_t> positions;
  std::vector<uint8_t> scales;
  std::vector<uint8_t> rotations;
  std::vector<uint8_t> alphas;
  std::vector<uint8_t> colors;
  std::vector<uint8_t> sh;

  bool usesFloat16() const;
  PackedGaussian at(int32_t i) const;
  UnpackedGaussian unpack(int32_t i, const CoordinateConverter &c) const;
};

struct PackOptions {
  CoordinateSystem from = CoordinateSystem::UNSPECIFIED;
};

struct UnpackOptions {
  CoordinateSystem to = CoordinateSystem::UNSPECIFIED;
};

// Saves Gaussian splat in packed format, no zlib compression, returning data in a std::string object.
LIBSPZ_API bool saveSpzUncompressed(const GaussianCloud &g, const PackOptions &o, uint8_t** ppOut, size_t* pOutSize);

// Loads Gaussian splat from a vector of bytes in packed format.
LIBSPZ_API GaussianCloud loadSpzUncompressed(const std::vector<uint8_t> &data, const UnpackOptions &options);

// Loads Gaussian splat from a vector of bytes in packed format.
LIBSPZ_API PackedGaussians loadSpzPacked(const std::string &filename);
LIBSPZ_API PackedGaussians loadSpzPacked(const uint8_t *data, int32_t size);
LIBSPZ_API PackedGaussians loadSpzPacked(const std::vector<uint8_t> &data);

// Saves Gaussian splat in packed format to a file
LIBSPZ_API bool saveSpz(
  const GaussianCloud &gaussians, const PackOptions &options, const std::string &filename);

// Loads Gaussian splat from a file in packed format
LIBSPZ_API GaussianCloud loadSpz(const std::string &filename, const UnpackOptions &o);

// Saves Gaussian splat data in .ply format
LIBSPZ_API bool saveSplatToPly(
  const spz::GaussianCloud &gaussians, const PackOptions &options, const std::string &filename);

// Loads Gaussian splat data in .ply format
LIBSPZ_API GaussianCloud loadSplatFromPly(const std::string &filename, const UnpackOptions &options);

// Loads Gaussian splat data from memory in .ply format 
LIBSPZ_API GaussianCloud loadSplatFromPlyData(const uint8_t* stream, size_t stream_size, const UnpackOptions& options);

// Feed in PLY stream and get uncompressed SPZ out. Need this because GaussianCloud struct isn't reliably interpreted across DLL boundary
// Also memory block pp_out_stream will be allocated using malloc() so use free() accordingly
LIBSPZ_API bool convertPlyDataToSpzData(const uint8_t* p_in_stream, size_t in_stream_size, const UnpackOptions& unpackOption, uint8_t** pp_out_stream, size_t* p_out_stream_size, const PackOptions& packOption);

LIBSPZ_API void serializePackedGaussians(const PackedGaussians &packed, std::ostream *out);

}  // namespace spz
