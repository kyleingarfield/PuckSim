#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
JPH_SUPPRESS_WARNINGS

namespace JPH {
	class BodyInterface;
}

using namespace JPH;

struct Goal {
	BodyID postsId;
	BodyID netColliderId;
	BodyID triggerId;
	BodyID playerColliderId;
};

Goal CreateGoal(BodyInterface& bi, bool isBlue);
void DestroyGoal(BodyInterface& bi, Goal& goal);
