#include "Player.h"
#include "Layers.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
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
	100.0f, 8.0f, 40.0f // PID gains, max force
};

const PlayerParams GOALIE_PARAMS = {
	5.0f, 6.25f, 5.0f, 6.0f,
	true, 1.0f, 5.1f, 5.0f,
	90.0f, 2.0f, 1.2f, 1.3f,
	100.0f, 8.0f, 40.0f
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
	CapsuleShape* capsule = new CapsuleShape(0.45f, 0.3f);
	RotatedTranslatedShapeSettings com_offset(Vec3(0, 1.25f, 0), Quat::sIdentity(), capsule);
	ShapeSettings::ShapeResult player_shape_result = com_offset.Create();
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

	
	BodyCreationSettings player_mesh_settings(player_shape, startPos, Quat::sIdentity(), EMotionType::Kinematic, Layers::PLAYER_MESH);
	player_mesh_settings.mFriction = 0.0f;
	player_mesh_settings.mRestitution = 0.2f; 

	BodyID player_mesh_id = bi.CreateAndAddBody(player_mesh_settings, EActivation::Activate);

	return { player_id, player_mesh_id, 0.0f, &params };
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