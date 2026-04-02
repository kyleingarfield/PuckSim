#pragma once
#include <Jolt/Jolt.h>

JPH_SUPPRESS_WARNINGS

#include <Jolt/Physics/Body/BodyID.h>

namespace JPH {
	class BodyInterface;
	class PhysicsSystem;
}

using namespace JPH;

struct PlayerParams {
	float maxForwardSpeed;
	float maxForwardSprintSpeed;
	float maxBackwardSpeed;
	float maxBackwardSprintSpeed;

	bool canDash;
	float slideDrag;
	float tackleSpeedThreshold;
	float tackleForceThreshold;

	float mass;
	float gravityMultiplier;
	float hoverTargetDistance;
	float hoverRaycastDistance;
	float hoverPGain;
	float hoverDGain;
	float hoverMaxForce;

	float forwardAcceleration;
	float backwardAcceleration;
	float brakeAcceleration;
	float drag;
	float overspeedDrag;

	float turnAcceleration;
	float turnBrakeAcceleration;
	float maxTurnSpeed;
	float turnDrag;
	float turnOverspeedDrag;

	float keepUprightPGain;
	float keepUprightDGain;

	float skateTraction;

	float velocityLeanLinearMult;
	float velocityLeanAngularMult;

	float minLaterality;
	float maxLaterality;
	float minLateralitySpeed;
	float maxLateralitySpeed;
};

extern const PlayerParams ATTACKER_PARAMS;
extern const PlayerParams GOALIE_PARAMS;

struct Player {
	BodyID bodyId;
	BodyID meshId;
	float prevHoverDist;
	const PlayerParams* params;
	Vec3 prevUp;
	float balance;
	float laterality;
};

struct PlayerInput {
	float forward;
	float turn;
	float lateral;
};
Player CreatePlayer(BodyInterface& bi, const PlayerParams& params, RVec3 startPos);

void DestroyPlayer(BodyInterface& bi, Player& player);

void UpdatePlayerHover(BodyInterface& bi, PhysicsSystem& ps, Player& player, float dt);

void SyncPlayerMesh(BodyInterface& bi, const Player& player, float dt);

void UpdateKeepUpright(BodyInterface& bi, Player& player, float dt);

void UpdateMovement(BodyInterface& bi, Player& player, const PlayerInput& input, float dt);

void UpdateSkate(BodyInterface& bi, Player& player, float dt);

void UpdateVelocityLean(BodyInterface& bi, Player& player, float dt);

void UpdateLaterality(BodyInterface& bi, Player& player, const PlayerInput& input, float dt);

