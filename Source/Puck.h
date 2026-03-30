#pragma once
#include <Jolt/Jolt.h>

JPH_SUPPRESS_WARNINGS

#include <Jolt/Physics/Body/BodyID.h>

namespace JPH {
	class BodyInterface;
	class PhysicsSystem;
}

using namespace JPH;

struct Puck {
	BodyID puckId;
	BodyID triggerId;
};

Puck CreatePuck(BodyInterface& bi, RVec3 startPos);
void DestroyPuck(BodyInterface& bi, Puck& puck);
void SyncPuckTrigger(BodyInterface& bi, const Puck& puck);
