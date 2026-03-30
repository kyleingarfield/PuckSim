#include "Stick.h"
#include "Layers.h"
#include "MeshData.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/Body.h>
#include <iostream>
using namespace std;

const StickParams ATTACKER_STICK_PARAMS = {
	Vec3(0, -0.006f, -0.992f), Vec3(0,0.118f, 1.328f), // shaft & blade local pos
	1.0f, // mass
	500.0f, 0, 0, 20.0f, 1.0f, 0.25f, // shaft gain/sat/smooth
	500.0f, 0, 0, 20.0f, 1.0f, // blade gain/sat/smooth
	20.0f, 0.3f, true // linear & angular vel
};

const StickParams GOALIE_STICK_PARAMS = {
	Vec3(0, -0.006f, -0.992f), Vec3(0,0.0902f, 1.3366f),
	1.0f,
	500.0f, 0, 0, 20.0f, 1.0f, 0.25f,
	500.0f, 0, 0, 20.0f, 1.0f,
	20.0f, 0.3f, true
};

static Vec3 UpdatePID(Vector3PID& state, float pGain, float iGain, float iSat, float dGain, float dSmoothing, float dt, Vec3 current, Vec3 target)
{
	Vec3 error = target - current;

	state.integral = state.integral + error * dt;
	state.integral.SetX(min(max(state.integral.GetX(), -iSat), iSat));
	state.integral.SetY(min(max(state.integral.GetY(), -iSat), iSat));
	state.integral.SetZ(min(max(state.integral.GetZ(), -iSat), iSat));

	Vec3 rawDeriv = (error - state.prevError) / dt;
	state.prevDerivative = state.prevDerivative * dSmoothing + rawDeriv * (1.0f - dSmoothing);

	Vec3 output = error * pGain + state.integral * iGain + state.prevDerivative * dGain;

	state.prevError = error;

	return output;
}

Stick CreateStick(BodyInterface& bi, const StickParams& params, RVec3 startPos)
{
	// BodyCreationSettings puck_settings(new SphereShape(0.152f), startPos, Quat::sIdentity(), EMotionType::Dynamic, Layers::PUCK);
	Array<Vec3> shaft_points;
	shaft_points.reserve(STICK_SHAFT_COLLIDER_NUM_VERTICES);
	for (int i = 0; i < STICK_SHAFT_COLLIDER_NUM_VERTICES; i++)
		shaft_points.push_back(Vec3(STICK_SHAFT_COLLIDER_VERTICES[i].x, STICK_SHAFT_COLLIDER_VERTICES[i].y, STICK_SHAFT_COLLIDER_VERTICES[i].z));
	ConvexHullShapeSettings shaft_hull(shaft_points);
	ShapeSettings::ShapeResult shaft_shape_result = shaft_hull.Create();
	ShapeRefC shaft = shaft_shape_result.Get();

	Array<Vec3> blade_points;
	blade_points.reserve(STICK_BLADE_COLLIDER_NUM_VERTICES);
	for (int i = 0; i < STICK_BLADE_COLLIDER_NUM_VERTICES; i++)
		blade_points.push_back(Vec3(STICK_BLADE_COLLIDER_VERTICES[i].x, STICK_BLADE_COLLIDER_VERTICES[i].y, STICK_BLADE_COLLIDER_VERTICES[i].z));
	ConvexHullShapeSettings blade_hull(blade_points);
	ShapeSettings::ShapeResult blade_shape_result = blade_hull.Create();
	ShapeRefC blade = blade_shape_result.Get();

	StaticCompoundShapeSettings compound;
	compound.AddShape(
		Vec3::sZero(),
		Quat::sIdentity(),
		shaft
	);

	compound.AddShape(
		Vec3::sZero(),
		Quat::sIdentity(),
		blade
	);

	ShapeSettings::ShapeResult stick_shape_result = compound.Create();
	if (stick_shape_result.HasError()) {
		cout << "Error Creating Compound Object";
	}
	ShapeRefC stick_shape = stick_shape_result.Get();


	BodyCreationSettings stick_settings(stick_shape, startPos, Quat::sIdentity(), EMotionType::Dynamic, Layers::STICK);
	// set body properties
	stick_settings.mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;
	stick_settings.mMassPropertiesOverride.mMass = params.mass;
	stick_settings.mMassPropertiesOverride.mInertia = Mat44::sIdentity();
	stick_settings.mGravityFactor = 0.0f;
	stick_settings.mFriction = 0.0f;
	stick_settings.mRestitution = 0.0f;
	stick_settings.mLinearDamping = 0.0f;
	stick_settings.mAngularDamping = 0.0f;
	stick_settings.mAllowSleeping = false;
	stick_settings.mMotionQuality = EMotionQuality::LinearCast;
	BodyID stickId = bi.CreateAndAddBody(stick_settings, EActivation::Activate);

	float length = (params.bladeHandleLocalPos - params.shaftHandleLocalPos).Length();

	return {
		stickId,
		&params,
		length,
		{ Vec3::sZero(), Vec3::sZero(), Vec3::sZero() },
		{ Vec3::sZero(), Vec3::sZero(), Vec3::sZero() },
		1.0f
	};
}

void DestroyStick(BodyInterface& bi, Stick& stick)
{
	bi.RemoveBody(stick.bodyId);
	bi.DestroyBody(stick.bodyId);
}

void UpdateStick(BodyInterface& bi, Stick& stick, Vec3 shaftTarget, Vec3 bladeTarget, BodyID playerBodyId, float dt)
{
	Vec3 stickPos = bi.GetPosition(stick.bodyId);
	Quat stickRot = bi.GetRotation(stick.bodyId);
	Vec3 shaftWorldPos = stickPos + (stickRot * stick.params->shaftHandleLocalPos);
	Vec3 bladeWorldPos = stickPos + (stickRot * stick.params->bladeHandleLocalPos);


	Vec3 shaftForce = UpdatePID(stick.shaftPID,
		stick.params->shaftPGain, stick.params->shaftIGain, stick.params->shaftISaturation,
		stick.params->shaftDGain, stick.params->shaftDSmoothing,
		dt, shaftWorldPos, shaftTarget

	);

	Vec3 bladeForce = UpdatePID(stick.bladePID,
		stick.params->bladePGain * stick.bladeGainMultiplier,
		stick.params->bladeIGain, stick.params->bladeISaturation,
		stick.params->bladeDGain, stick.params->bladeDSmoothing,
		dt, bladeWorldPos, bladeTarget
	);

	Vec3 playerLinVel = bi.GetLinearVelocity(playerBodyId);
	Vec3 playerAngVel = bi.GetAngularVelocity(playerBodyId);
	RVec3 playerCOM = bi.GetCenterOfMassPosition(playerBodyId);

	Vec3 shaftVel = playerLinVel + playerAngVel.Cross(Vec3(RVec3(shaftWorldPos) - playerCOM));
	Vec3 bladeVel = playerLinVel + playerAngVel.Cross(Vec3(RVec3(bladeWorldPos) - playerCOM));

	bi.AddImpulse(stick.bodyId, shaftVel * stick.params->linearVelTransferMult * dt, RVec3(shaftWorldPos));
	bi.AddImpulse(stick.bodyId, bladeVel * stick.params->linearVelTransferMult * dt, RVec3(bladeWorldPos));

	bi.AddImpulse(stick.bodyId, shaftForce * dt, RVec3(shaftWorldPos));
	bi.AddImpulse(stick.bodyId, bladeForce * dt, RVec3(bladeWorldPos));

	Vec3 stickAngVel = bi.GetAngularVelocity(stick.bodyId);
	Vec3 localAngVel = stickRot.Conjugated() * stickAngVel;
	localAngVel.SetZ(0.0f);
	bi.SetAngularVelocity(stick.bodyId, stickRot * localAngVel);

	Vec3 euler = stickRot.GetEulerAngles();
	Quat fixedRot = Quat::sEulerAngles(Vec3(euler.GetX(), euler.GetY(), 0.0f));
	bi.SetRotation(stick.bodyId, fixedRot, EActivation::DontActivate);

	// Transfer stick angular velocity back to player body
	// Unity: AddTorque(-scaledAngVel, ForceMode.Acceleration) = angular acceleration applied over dt
	// Direct velocity change: deltaAngVel = acceleration * dt
	if (stick.params->transferAngularVelocity)
	{
		Vec3 scaledAngVel = stickAngVel * Vec3(0.5f, 1.0f, 0.0f) *
			stick.params->angularVelTransferMult;
		Vec3 curPlayerAngVel = bi.GetAngularVelocity(playerBodyId);
		bi.SetAngularVelocity(playerBodyId, curPlayerAngVel - scaledAngVel * dt);
	}

	stick.bladeGainMultiplier = 1.0f;

}