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
	BodyID boardsId;
};

Rink CreateRink(BodyInterface& bi);
void DestroyRink(BodyInterface& bi, Rink& rink);
