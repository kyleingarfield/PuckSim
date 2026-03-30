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
};

extern const PlayerParams ATTACKER_PARAMS;
extern const PlayerParams GOALIE_PARAMS;

struct Player {
	BodyID bodyId;
	BodyID meshId;
	float prevHoverDist;
	const PlayerParams* params;
};

Player CreatePlayer(BodyInterface& bi, const PlayerParams& params, RVec3 startPos);

void DestroyPlayer(BodyInterface& bi, Player& player);

void UpdatePlayerHover(BodyInterface& bi, PhysicsSystem& ps, Player& player, float dt);

void SyncPlayerMesh(BodyInterface& bi, const Player& player, float dt);
