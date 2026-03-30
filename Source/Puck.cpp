#include "Puck.h"
#include "Layers.h"
#include "MeshData.h"
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>

JPH_SUPPRESS_WARNINGS

Puck CreatePuck(BodyInterface& bi, RVec3 startPos)
{
	// PUCK
	Array<Vec3> puck_points;
	puck_points.reserve(PUCK_LEVEL_COLLIDER_NUM_VERTICES);
	for (int i = 0; i < PUCK_LEVEL_COLLIDER_NUM_VERTICES; i++)
		puck_points.push_back(Vec3(PUCK_LEVEL_COLLIDER_VERTICES[i].x, PUCK_LEVEL_COLLIDER_VERTICES[i].y, PUCK_LEVEL_COLLIDER_VERTICES[i].z));
	ConvexHullShapeSettings puck_hull(puck_points);
	ShapeSettings::ShapeResult puck_shape_result = puck_hull.Create();
	ShapeRefC puck_shape = puck_shape_result.Get();

	BodyCreationSettings puck_settings(puck_shape, startPos, Quat::sIdentity(), EMotionType::Dynamic, Layers::PUCK);
	puck_settings.mFriction = 0.0f;
	puck_settings.mRestitution = 0.0f;
	puck_settings.mLinearDamping = 0.3f;
	puck_settings.mAngularDamping = 0.3f;
	puck_settings.mMaxLinearVelocity = 30.f;
	puck_settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
	puck_settings.mMassPropertiesOverride.mMass = 0.375f;
	//puck_settings.mMotionQuality = EMotionQuality::LinearCast;
	BodyID puck_id = bi.CreateAndAddBody(puck_settings, EActivation::Activate);

	// PUCK TRIGGER
	BodyCreationSettings puck_trigger_settings(new SphereShape(0.15f), startPos, Quat::sIdentity(), EMotionType::Kinematic, Layers::PUCK_TRIGGER);
	puck_trigger_settings.mIsSensor = true;
	BodyID puck_trigger_id = bi.CreateAndAddBody(puck_trigger_settings, EActivation::Activate);

	return { puck_id, puck_trigger_id };
}

void DestroyPuck(BodyInterface& bi, Puck& puck)
{
	// Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added at any time.
	bi.RemoveBody(puck.puckId);
	bi.RemoveBody(puck.triggerId);
	// Destroy the sphere. After this the sphere ID is no longer valid.
	bi.DestroyBody(puck.puckId);
	bi.DestroyBody(puck.triggerId);
}

void SyncPuckTrigger(BodyInterface& bi, const Puck& puck)
{
	bi.SetPosition(puck.triggerId, bi.GetPosition(puck.puckId), EActivation::DontActivate);
}
