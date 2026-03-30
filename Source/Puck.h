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
	bool isTouchingStick;
	bool isGrounded;
};

Puck CreatePuck(BodyInterface& bi, RVec3 startPos);
void DestroyPuck(BodyInterface& bi, Puck& puck);
void SyncPuckTrigger(BodyInterface& bi, const Puck& puck);
void UpdatePuckPostStickExit(BodyInterface& bi, Puck& puck, float dt);
void UpdatePuckGroundCheck(BodyInterface& bi, PhysicsSystem& ps, Puck& puck);
void UpdatePuckTensor(PhysicsSystem& ps, Puck& puck);

