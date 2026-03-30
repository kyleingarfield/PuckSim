#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
JPH_SUPPRESS_WARNINGS

namespace JPH {
	class BodyInterface;
	class PhysicsSystem;
}

using namespace JPH;

struct Rink {
	BodyID iceId;
	BodyID leftWallId;
	BodyID rightWallId;
	BodyID farWallId;
	BodyID nearWallId;
	BodyID blueGoalTriggerId;
	BodyID redGoalTriggerId;
};

Rink CreateRink(BodyInterface& bi);
void DestroyRink(BodyInterface& bi, Rink& rink);
