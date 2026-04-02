#include "Player.h"
#include "Layers.h"
#include "MeshData.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/RegisterTypes.h>


JPH_SUPPRESS_WARNINGS

const PlayerParams ATTACKER_PARAMS = {
	7.5f, 8.75f, 7.5f, 8.5f, // speed caps
	false, 0.1f, 7.6f, 7.0f, // gameplay
	90.0f, 2.0f, 1.2f, 1.3f, // mass, gravity, hover dist/raycast
	100.0f, 8.0f, 40.0f, // PID gains, max force
	2.0f, 1.8f, 5.0f, 0.015f, 0.25f,
	1.5f, 4.5f, 1.3125f, 3.0f, 2.25f,
	50.0f, 8.0f,
	12.5f,
	0.75f, 6.0f,
	0.5f, 1.0f, 2.0f, 5.0f // laterality
};

const PlayerParams GOALIE_PARAMS = {
	5.0f, 6.25f, 5.0f, 6.0f,
	true, 1.0f, 5.1f, 5.0f,
	90.0f, 2.0f, 1.2f, 1.3f,
	100.0f, 8.0f, 40.0f,
	2.0f, 1.8f, 5.0f, 0.015f, 0.25f,
	1.5f, 4.5f, 1.3125f, 3.0f, 2.25f,
	50.0f, 8.0f,
	12.5f,
	0.75f, 6.0f,
	0.5f, 1.0f, 2.0f, 5.0f // laterality
};
namespace {
	class IceLayerFilter : public ObjectLayerFilter
	{
	public:
		bool ShouldCollide(ObjectLayer inLayer) const override
		{
			return inLayer == Layers::ICE;
		}
	};
}

Player CreatePlayer(BodyInterface& bi, const PlayerParams& params, RVec3 startPos)
{
	// PLAYER
	StaticCompoundShapeSettings player_compound;
	player_compound.AddShape(
		Vec3(0, 1.25f, 0),
		Quat::sIdentity(),
		new CapsuleShape(0.45f, 0.3f)
	);
	player_compound.AddShape(
		Vec3(0, 1.5f, 0),
		Quat::sIdentity(),
		new SphereShape(0.4f)
	);
	ShapeSettings::ShapeResult player_shape_result = player_compound.Create();
	ShapeRefC player_shape = player_shape_result.Get();
	BodyCreationSettings player_settings(player_shape, startPos, Quat::sIdentity(), EMotionType::Dynamic, Layers::PLAYER);

	player_settings.mGravityFactor = params.gravityMultiplier;
	player_settings.mFriction = 0.0f;
	player_settings.mRestitution = 0.2f;
	player_settings.mLinearDamping = 0.0f;
	player_settings.mAngularDamping = 0.0f;
	player_settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
	player_settings.mMassPropertiesOverride.mMass = params.mass;
	player_settings.mAllowSleeping = false;

	BodyID player_id = bi.CreateAndAddBody(player_settings, EActivation::Activate);

	
	Quat rot90x = Quat::sRotation(Vec3::sAxisX(), 0.5f * JPH_PI);
	Array<Vec3> groin_points;
	groin_points.reserve(GROIN_COLLIDER_NUM_VERTICES);
	for (int i = 0; i < GROIN_COLLIDER_NUM_VERTICES; i++)
		groin_points.push_back(Vec3(GROIN_COLLIDER_VERTICES[i].x, GROIN_COLLIDER_VERTICES[i].y, GROIN_COLLIDER_VERTICES[i].z));
	ConvexHullShapeSettings groin_hull(groin_points);
	ShapeRefC groin_shape = groin_hull.Create().Get();

	Array<Vec3> torso_points;
	torso_points.reserve(TORSO_COLLIDER_NUM_VERTICES);
	for (int i = 0; i < TORSO_COLLIDER_NUM_VERTICES; i++)
		torso_points.push_back(Vec3(TORSO_COLLIDER_VERTICES[i].x, TORSO_COLLIDER_VERTICES[i].y, TORSO_COLLIDER_VERTICES[i].z));
	ConvexHullShapeSettings torso_hull(torso_points);
	ShapeRefC torso_shape = torso_hull.Create().Get();

	StaticCompoundShapeSettings mesh_compound;
	mesh_compound.AddShape(Vec3(0, 0.48f, 0), rot90x, groin_shape);
	mesh_compound.AddShape(Vec3(0, 1.03f, 0), rot90x, torso_shape);
	mesh_compound.AddShape(Vec3(0, 1.875f, 0.025f), Quat::sIdentity(), new SphereShape(0.225f));

	ShapeSettings::ShapeResult mesh_shape_result = mesh_compound.Create();
	ShapeRefC mesh_shape = mesh_shape_result.Get();

	BodyCreationSettings player_mesh_settings(mesh_shape, startPos, Quat::sIdentity(), EMotionType::Kinematic, Layers::PLAYER_MESH);
	player_mesh_settings.mFriction = 0.0f;
	player_mesh_settings.mRestitution = 0.2f; 

	BodyID player_mesh_id = bi.CreateAndAddBody(player_mesh_settings, EActivation::Activate);

	return { player_id, player_mesh_id, 0.0f, &params , Vec3(0, 1, 0), 1.0f, 0.0f };
}

void DestroyPlayer(BodyInterface& bi, Player& player)
{
	bi.RemoveBody(player.bodyId);
	bi.DestroyBody(player.bodyId);
	bi.RemoveBody(player.meshId);
	bi.DestroyBody(player.meshId);
}

void UpdatePlayerHover(BodyInterface& bi, PhysicsSystem& ps, Player& player, float dt)
{	
	IceLayerFilter ice_filter;

	// Raycast down to find dist to ice
	RVec3 origin = bi.GetPosition(player.bodyId) + Vec3(0, 1.0f, 0);
	RRayCast ray = RRayCast(origin, Vec3(0, -player.params->hoverRaycastDistance, 0));

	float currentDistance = player.params->hoverRaycastDistance;

	RayCastResult hit;

	if (ps.GetNarrowPhaseQuery().CastRay(ray, hit, {}, ice_filter))
	{
		currentDistance = hit.mFraction * player.params->hoverRaycastDistance;
	}

	float error = player.params->hoverTargetDistance - currentDistance;
	float valueDerivative = (currentDistance - player.prevHoverDist) / dt;
	float P_term = player.params->hoverPGain * error;
	float D_term = player.params->hoverDGain * (-valueDerivative);
	float output = P_term + D_term;

	float force = output < 0.0f ? 0.0f : (output > player.params->hoverMaxForce ? player.params->hoverMaxForce : output);
	bi.AddForce(player.bodyId, Vec3(0, force * player.params->mass, 0));

	player.prevHoverDist = currentDistance;
}

void SyncPlayerMesh(BodyInterface& bi, const Player& player, float dt)
{
	bi.MoveKinematic(player.meshId, bi.GetPosition(player.bodyId), bi.GetRotation(player.bodyId), dt);
}

void UpdateKeepUpright(BodyInterface& bi, Player& player, float dt)
{
	Quat rot = bi.GetRotation(player.bodyId);
	Vec3 currentUp = rot * Vec3(0, 1, 0);

	Vec3 error = Vec3(0, 1, 0) - currentUp;
	Vec3 valueDerivate = (currentUp - player.prevUp) / dt;
	Vec3 pidOutput = player.params->keepUprightPGain * player.balance * error
		+ player.params->keepUprightDGain * player.balance * (-valueDerivate);

	Vec3 torqueAxis = pidOutput.Cross(Vec3(0, 1, 0));
	Vec3 angVel = bi.GetAngularVelocity(player.bodyId);
	angVel += -torqueAxis * dt;
	bi.SetAngularVelocity(player.bodyId, angVel);

	player.prevUp = currentUp;
}

void UpdateMovement(BodyInterface& bi, Player& player, const PlayerInput& input, float dt)
{
	Vec3 vel = bi.GetLinearVelocity(player.bodyId);
	float speed = Vec3(vel.GetX(), 0, vel.GetZ()).Length();

	Quat rot = bi.GetRotation(player.bodyId);

	// Rotate movement direction by laterality (slerp between left, forward, right)
	Quat lateralRot = Quat::sRotation(Vec3(0, 1, 0), -player.laterality * 0.5f * JPH_PI);
	Vec3 forward = (rot * lateralRot) * Vec3(0, 0, 1);

	float accel = 0.0f;
	float forwardSpeed = forward.Dot(vel);  // positive = moving forward, negative = backward

	if (input.forward > 0.0f) {
		if (forwardSpeed < 0.0f)  // moving backward, input forward = braking
			accel = player.params->brakeAcceleration;
		else if (speed < player.params->maxForwardSpeed)
			accel = player.params->forwardAcceleration;
	}
	else if (input.forward < 0.0f) {
		if (forwardSpeed > 0.0f)  // moving forward, input backward = braking
			accel = -player.params->brakeAcceleration;
		else if (speed < player.params->maxBackwardSpeed)
			accel = player.params->backwardAcceleration * input.forward;
	}

	bi.AddForce(player.bodyId, forward * accel * player.params->mass);

	vel = bi.GetLinearVelocity(player.bodyId);
	float maxSpeed = (input.forward >= 0.0f) ? player.params->maxForwardSpeed : player.params->maxBackwardSpeed;

	if (speed > maxSpeed) {
		vel *= (1.0f - player.params->overspeedDrag * dt);
	}
	else { vel *= (1.0f - player.params->drag * dt); }

	bi.SetLinearVelocity(player.bodyId, vel);

	Vec3 angVel = bi.GetAngularVelocity(player.bodyId);
	Vec3 localAngVel = rot.Conjugated() * angVel;  // world to local
	float turnSpeed = localAngVel.GetY();
	turnSpeed = turnSpeed < 0 ? -turnSpeed : turnSpeed;

	if (input.turn != 0.0f) {
		bool sameDirection = (input.turn > 0.0f) == (localAngVel.GetY() > 0.0f);

		float turnAccel;

		if (sameDirection && turnSpeed < player.params->maxTurnSpeed) {
			turnAccel = player.params->turnAcceleration * input.turn;
		}
		else if (!sameDirection) {
			turnAccel = player.params->turnBrakeAcceleration * input.turn;
		}
		else { turnAccel = 0.0f; }

		angVel += Vec3(0, turnAccel * dt, 0);  // world Y axis
	}

	float dragFactor;
	if (turnSpeed > player.params->maxTurnSpeed) {
		dragFactor = 1.0f - player.params->turnOverspeedDrag * dt;
	}
	else if (input.turn == 0.0f) {
		dragFactor = 1.0f - player.params->turnDrag * dt;
	}
	else { dragFactor = 1.0f; }

	angVel = Vec3(angVel.GetX(), angVel.GetY() * dragFactor, angVel.GetZ());
	bi.SetAngularVelocity(player.bodyId, angVel);
}

void UpdateSkate(BodyInterface& bi, Player& player, float dt)
{
	Vec3 vel = bi.GetLinearVelocity(player.bodyId);
	Quat rot = bi.GetRotation(player.bodyId);

	// Skate traction operates in the laterality-rotated movement direction
	Quat lateralRot = Quat::sRotation(Vec3(0, 1, 0), -player.laterality * 0.5f * JPH_PI);
	Quat movementRot = rot * lateralRot;
	Vec3 localVel = movementRot.Conjugated() * vel;

	float lateralDrift = -localVel.GetX();
	float maxCorrection = player.params->skateTraction * dt;
	float correction = (lateralDrift > maxCorrection) ? maxCorrection :
		(lateralDrift < -maxCorrection) ? -maxCorrection : lateralDrift;

	Vec3 worldRight = movementRot * Vec3(1, 0, 0);
	vel += worldRight * correction * player.balance;
	bi.SetLinearVelocity(player.bodyId, vel);
}

void UpdateVelocityLean(BodyInterface& bi, Player& player, float dt)
{
	Vec3 vel = bi.GetLinearVelocity(player.bodyId);
	Quat rot = bi.GetRotation(player.bodyId);
	Vec3 localVel = rot.Conjugated() * vel;
	float forwardSpeed = localVel.GetZ();

	Vec3 angVel = bi.GetAngularVelocity(player.bodyId);
	Vec3 localAngVel = rot.Conjugated() * angVel;
	float turnRate = localAngVel.GetY();

	Vec3 worldRight = rot * Vec3(1, 0, 0);
	Vec3 worldForward = rot * Vec3(0, 0, 1);

	float speed = Vec3(vel.GetX(), 0, vel.GetZ()).Length();
	float maxSpeed = player.params->maxForwardSpeed > player.params->maxBackwardSpeed
		? player.params->maxForwardSpeed : player.params->maxBackwardSpeed;
	float linearIntensity = speed / maxSpeed;
	if (linearIntensity < 0.1f) linearIntensity = 0.1f;

	Vec3 rollTorque = worldRight * forwardSpeed * player.params->velocityLeanLinearMult * linearIntensity;
	Vec3 pitchTorque = -worldForward * turnRate * player.params->velocityLeanAngularMult * linearIntensity;

	angVel += (rollTorque + pitchTorque) * dt;
	bi.SetAngularVelocity(player.bodyId, angVel);
}

void UpdateLaterality(BodyInterface& bi, Player& player, const PlayerInput& input, float dt)
{
	Vec3 vel = bi.GetLinearVelocity(player.bodyId);
	float speed = Vec3(vel.GetX(), 0, vel.GetZ()).Length();

	float minSpeed = player.params->maxForwardSpeed < player.params->maxBackwardSpeed
		? player.params->maxForwardSpeed : player.params->maxBackwardSpeed;
	float normalizedMinSpeed = speed / minSpeed;
	if (normalizedMinSpeed > 1.0f) normalizedMinSpeed = 1.0f;

	// t = 1 - normalizedMinSpeed: high at low speed, low at high speed
	float t = 1.0f - normalizedMinSpeed;
	if (t < 0.0f) t = 0.0f;

	// Lerp speed and magnitude based on t
	float lateralitySpeed = player.params->minLateralitySpeed
		+ (player.params->maxLateralitySpeed - player.params->minLateralitySpeed) * t;
	float lateralityMag = player.params->minLaterality
		+ (player.params->maxLaterality - player.params->minLaterality) * t;

	// Determine target based on input
	float target;
	if (input.lateral < 0.0f)
		target = -lateralityMag;
	else if (input.lateral > 0.0f)
		target = lateralityMag;
	else
		target = 0.0f;

	// Lerp toward target: current + (target - current) * speed * dt
	player.laterality += (target - player.laterality) * lateralitySpeed * dt;
}
