#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>

JPH_SUPPRESS_WARNINGS

using namespace JPH;

namespace Layers
{
	static constexpr ObjectLayer DEFAULT = 0;
	static constexpr ObjectLayer TRANSPARENT_FX = 1;
	static constexpr ObjectLayer IGNORE_RAYCAST = 2;
	static constexpr ObjectLayer LEVEL = 3;
	static constexpr ObjectLayer WATER = 4;
	static constexpr ObjectLayer UI = 5;
	static constexpr ObjectLayer STICK = 6;
	static constexpr ObjectLayer PUCK = 7;
	static constexpr ObjectLayer PLAYER = 8;
	static constexpr ObjectLayer PLAYER_MESH = 9;
	static constexpr ObjectLayer POST_PROCESSING = 10;
	static constexpr ObjectLayer GOAL_POST = 11;
	static constexpr ObjectLayer BOARDS = 12;
	static constexpr ObjectLayer ICE = 13;
	static constexpr ObjectLayer GOAL_NET = 14;
	static constexpr ObjectLayer GOAL_TRIGGER = 15;
	static constexpr ObjectLayer PLAYER_COLLIDER = 16;
	static constexpr ObjectLayer NET_CLOTH_COLLIDER = 17;
	static constexpr ObjectLayer GOAL_FRAME = 18;
	static constexpr ObjectLayer PUCK_TRIGGER = 19;
	static constexpr ObjectLayer NUM_LAYERS = 20;
};

namespace BroadPhaseLayers
{
	static constexpr BroadPhaseLayer STATIC(0);
	static constexpr BroadPhaseLayer DYNAMIC(1);
	static constexpr BroadPhaseLayer SENSOR(2);
	static constexpr uint NUM_LAYERS(3);
};

void SetupBroadPhaseMapping(BroadPhaseLayerInterfaceTable& bpli);
void SetupCollisionPairs(ObjectLayerPairFilterTable& filter);
