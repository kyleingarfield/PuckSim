#include "Puck.h"
#include "Layers.h"
#include "MeshData.h"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionProperties.h>

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
	puck_settings.mMaxAngularVelocity = 60.f;
	puck_settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
	puck_settings.mMassPropertiesOverride.mMass = 0.375f;
	//puck_settings.mMotionQuality = EMotionQuality::LinearCast;
	BodyID puck_id = bi.CreateAndAddBody(puck_settings, EActivation::Activate);

	// PUCK TRIGGER
	BodyCreationSettings puck_trigger_settings(new SphereShape(0.15f), startPos, Quat::sIdentity(), EMotionType::Kinematic, Layers::PUCK_TRIGGER);
	puck_trigger_settings.mIsSensor = true;
	BodyID puck_trigger_id = bi.CreateAndAddBody(puck_trigger_settings, EActivation::Activate);

	return { puck_id, puck_trigger_id, false, false };
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

void UpdatePuckPostStickExit(BodyInterface& bi, Puck& puck, float dt)
{
	Vec3 vel = bi.GetLinearVelocity(puck.puckId);

	// Y-axis correction: kill upward velocity
	// Unity: AddForce((0, min(0, -vel.y), 0) * 5, Acceleration)
	// Acceleration applied over one dt: deltaV = acceleration * dt
	float yVel = vel.GetY();
	if (yVel > 0.0f)
	{
		float deltaV = -yVel * 5.0f * dt;
		vel.SetY(yVel + deltaV);
		bi.SetLinearVelocity(puck.puckId, vel);
	}

	// Clamp linear velocity to 30 m/s
	vel = bi.GetLinearVelocity(puck.puckId);
	if (vel.Length() > 30.0f)
		bi.SetLinearVelocity(puck.puckId, vel.Normalized() * 30.0f);

	// Clamp angular velocity to 60 rad/s
	Vec3 angVel = bi.GetAngularVelocity(puck.puckId);
	if (angVel.Length() > 60.0f)
		bi.SetAngularVelocity(puck.puckId, angVel.Normalized() * 60.0f);
}

namespace {
	class IceOnlyLayerFilter : public ObjectLayerFilter
	{
	public:
		bool ShouldCollide(ObjectLayer inLayer) const override
		{
			return inLayer == Layers::ICE;
		}
	};
}

void UpdatePuckGroundCheck(BodyInterface& bi, PhysicsSystem& ps, Puck& puck)
{
	// Sphere overlap at puck position, radius 0.075, ice layer only
	// Replicates Unity Physics.CheckSphere(transform.position, 0.075, iceLayerMask)
	IceOnlyLayerFilter ice_filter;
	RVec3 puckPos = bi.GetPosition(puck.puckId);

	AllHitCollisionCollector<CollideShapeCollector> collector;
	SphereShape checkSphere(0.075f);
	CollideShapeSettings settings;
	ps.GetNarrowPhaseQuery().CollideShape(
		&checkSphere,
		Vec3::sReplicate(1.0f),
		RMat44::sTranslation(puckPos),
		settings,
		puckPos,
		collector,
		{},
		ice_filter
	);

	puck.isGrounded = collector.HadHit();

	// Grounded COM shift: simulate (0, -0.01, 0) local offset via gravitational torque
	// Torque = comOffset_world × gravityForce = (rot * localOffset) × (0, -m*g, 0)
	if (puck.isGrounded)
	{
		Quat rot = bi.GetRotation(puck.puckId);
		Vec3 comWorld = rot * Vec3(0.0f, -0.01f, 0.0f);
		Vec3 gravityForce = Vec3(0.0f, -9.81f * 0.375f, 0.0f);
		bi.AddTorque(puck.puckId, comWorld.Cross(gravityForce));
	}
}

void UpdatePuckTensor(PhysicsSystem& ps, Puck& puck)
{
	BodyLockWrite lock(ps.GetBodyLockInterface(), puck.puckId);
	if (lock.Succeeded())
	{
		Body& body = lock.GetBody();
		MassProperties mp;
		mp.mMass = 0.375f;

		if (puck.isTouchingStick)
			mp.mInertia = Mat44::sScale(Vec3(0.006f, 0.002f, 0.006f));
		else
			mp.mInertia = Mat44::sScale(Vec3(0.002f, 0.002f, 0.002f));

		body.GetMotionProperties()->SetMassProperties(EAllowedDOFs::All, mp);
	}
}