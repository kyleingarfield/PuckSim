#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
JPH_SUPPRESS_WARNINGS

namespace JPH {
	class BodyInterface;
	class PhysicsSystem;
}

using namespace JPH;

struct StickParams {
	Vec3 shaftHandleLocalPos;
	Vec3 bladeHandleLocalPos;

	float mass;

	float shaftPGain;
	float shaftIGain;
	float shaftISaturation;
	float shaftDGain;
	float shaftDSmoothing;
	float minShaftPGainMult;

	float bladePGain;
	float bladeIGain;
	float bladeISaturation;
	float bladeDGain;
	float bladeDSmoothing;

	float linearVelTransferMult;
	float angularVelTransferMult;
	bool transferAngularVelocity;
};

extern const StickParams ATTACKER_STICK_PARAMS;
extern const StickParams GOALIE_STICK_PARAMS;

struct Vector3PID {
	Vec3 integral;
	Vec3 prevError;
	Vec3 prevDerivative;
};

struct Stick {
	BodyID bodyId;
	const StickParams* params;
	float length;

	Vector3PID shaftPID;
	Vector3PID bladePID;

	float bladeGainMultiplier;
};

Stick CreateStick(BodyInterface& bi, const StickParams& params, RVec3 startPos);
void DestroyStick(BodyInterface& bi, Stick& stick);
void UpdateStick(BodyInterface& bi, Stick& stick, Vec3 shaftTarget, Vec3 bladeTarget, BodyID playerBodyId, float dt);