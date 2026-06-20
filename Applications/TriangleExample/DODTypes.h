// Copyright (c) 2026 HardBop Engine. All rights reserved.
#pragma once

#include <cstdint>
#include <vector>
#include "Engine/Math/Matrix.h"

namespace Math = hbe;

namespace hbe
{
namespace TriangleExample
{

// ============================================================================
// 64-BIT SORT KEY - Radix Sort Layout
// Bit layout: [31-0] Depth | [32-47] Material Index | [48-59] PSO Hash | [63] PassID
// ============================================================================
struct alignas(8) RenderSortKey
{
public:
	uint64_t rawKey;

	constexpr explicit RenderSortKey(uint64_t inKey = 0) noexcept
		: rawKey(inKey)
	{}

	static constexpr RenderSortKey Make(uint8_t inPassId, uint16_t inPsoHash, uint16_t inMatIdx,
										uint32_t inDepth) noexcept
	{
		uint64_t key = 0;
		key |= (static_cast<uint64_t>(inPassId & 1) << 63);
		key |= (static_cast<uint64_t>(inPsoHash & 0xFFF) << 48);
		key |= (static_cast<uint64_t>(inMatIdx & 0xFFFF) << 32);
		key |= (inDepth & 0xFFFFFFFF);
		return RenderSortKey(key);
	}

	[[nodiscard]] bool operator<(const RenderSortKey& other) const noexcept
	{
		return rawKey < other.rawKey;
	}

	[[nodiscard]] bool operator==(const RenderSortKey& other) const noexcept
	{
		return rawKey == other.rawKey;
	}
};

// ============================================================================
// DRAW COMMAND - GPU-like layout (Indirect Draw)
// ============================================================================
struct alignas(32) DrawCommand
{
public:
	RenderSortKey sortKey;
	uint32_t instanceCount;
	uint32_t firstInstance;
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t vertexOffset;
	uint32_t meshHandle;

	DrawCommand() noexcept
		: sortKey(0)
		, instanceCount(0)
		, firstInstance(0)
		, indexCount(0)
		, firstIndex(0)
		, vertexOffset(0)
		, meshHandle(0)
	{}
};

// ============================================================================
// INSTANCE DATA - Structure of Arrays (SoA) for SIMD/GPU efficiency
// ============================================================================
class InstanceData final
{
public:
	void Resize(size_t inCount)
	{
		worldMatrices.resize(inCount);
		colors.resize(inCount);
	}

	void Clear() noexcept
	{
		worldMatrices.clear();
		colors.clear();
	}

	[[nodiscard]] size_t Size() const noexcept
	{
		return worldMatrices.size();
	}

	std::vector<Math::TFloat4x4> worldMatrices;
	std::vector<Math::TFloat4> colors;
};

// ============================================================================
// MESH DATA
// ============================================================================
struct Vertex
{
	Math::TFloat3 position;
	Math::TFloat4 color;
};

static const Vertex TriangleMesh[] = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
									  {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
									  {{0.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}};

// ============================================================================
// PROJECTED DATA - Result of PREPARE phase
// ============================================================================
struct ProjectedTriangle
{
	Math::TFloat3 v0, v1, v2;
	Math::TFloat4 color;
	float invArea;
};

struct PrepareBuffers
{
	std::vector<ProjectedTriangle> triangles;

	void Clear() noexcept
	{
		triangles.clear();
	}
};

} // namespace TriangleExample
} // namespace hbe
