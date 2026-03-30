#include "Rink.h"
#include "Layers.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;

Rink CreateRink(BodyInterface& bi)
{
	// ICE
	BoxShapeSettings ice_shape_settings(Vec3(15.0f, 0.0125f, 39.0f));
	ice_shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked as such to prevent it from being freed when its reference count goes to 0.
	ShapeSettings::ShapeResult ice_shape_result = ice_shape_settings.Create();
	ShapeRefC ice_shape = ice_shape_result.Get();
	BodyCreationSettings ice_settings(ice_shape, RVec3(0.0_r, 0.0125_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::ICE);
	ice_settings.mFriction = 0.0f;
	ice_settings.mRestitution = 0.0f;
	BodyID iceId = bi.CreateAndAddBody(ice_settings, EActivation::DontActivate);

	// LEFT WALL
	BoxShapeSettings left_wall_shape_settings(Vec3(0.4f, 12.5f, 30.5f));
	left_wall_shape_settings.SetEmbedded();
	ShapeSettings::ShapeResult left_wall_shape_result = left_wall_shape_settings.Create();
	ShapeRefC left_wall_shape = left_wall_shape_result.Get();
	BodyCreationSettings left_wall_settings(left_wall_shape, RVec3(-19.25_r, 12.5_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	left_wall_settings.mFriction = 0.0f;
	left_wall_settings.mRestitution = 0.2f;
	BodyID leftWallId = bi.CreateAndAddBody(left_wall_settings, EActivation::DontActivate);

	// RIGHT WALL
	BoxShapeSettings right_wall_shape_settings(Vec3(0.4f, 12.5f, 30.5f));
	right_wall_shape_settings.SetEmbedded();
	ShapeSettings::ShapeResult right_wall_shape_result = right_wall_shape_settings.Create();
	ShapeRefC right_wall_shape = right_wall_shape_result.Get();
	BodyCreationSettings right_wall_settings(right_wall_shape, RVec3(19.25_r, 12.5_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	right_wall_settings.mFriction = 0.0f;
	right_wall_settings.mRestitution = 0.2f;
	BodyID rightWallId = bi.CreateAndAddBody(right_wall_settings, EActivation::DontActivate);

	// FAR END WALL
	BoxShapeSettings far_wall_shape_settings(Vec3(19.65f, 12.5f, 2.0f));
	far_wall_shape_settings.SetEmbedded();
	ShapeSettings::ShapeResult far_wall_shape_result = far_wall_shape_settings.Create();
	ShapeRefC far_wall_shape = far_wall_shape_result.Get();
	BodyCreationSettings far_wall_settings(far_wall_shape, RVec3(0.0_r, 12.5_r, 39.0_r), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	far_wall_settings.mFriction = 0.0f;
	far_wall_settings.mRestitution = 0.2f;
	BodyID farWallId = bi.CreateAndAddBody(far_wall_settings, EActivation::DontActivate);

	// NEAR END WALL
	BoxShapeSettings near_wall_shape_settings(Vec3(19.65f, 12.5f, 2.0f));
	near_wall_shape_settings.SetEmbedded();
	ShapeSettings::ShapeResult near_wall_shape_result = near_wall_shape_settings.Create();
	ShapeRefC near_wall_shape = near_wall_shape_result.Get();
	BodyCreationSettings near_wall_settings(near_wall_shape, RVec3(0.0_r, 12.5_r, -39.0_r), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	near_wall_settings.mFriction = 0.0f;
	near_wall_settings.mRestitution = 0.2f;
	BodyID nearWallId = bi.CreateAndAddBody(near_wall_settings, EActivation::DontActivate);

	// BLUE GOAL TRIGGER
	BoxShapeSettings blue_goal_trigger_shape_settings(Vec3(1.515f, 0.745f, 0.25f));
	blue_goal_trigger_shape_settings.SetEmbedded();
	ShapeSettings::ShapeResult blue_goal_trigger_shape_result = blue_goal_trigger_shape_settings.Create();
	ShapeRefC blue_goal_trigger_shape = blue_goal_trigger_shape_result.Get();
	BodyCreationSettings blue_goal_trigger_settings(blue_goal_trigger_shape, RVec3(0.0_r, 0.745_r, 34.0_r), Quat::sIdentity(), EMotionType::Static, Layers::GOAL_TRIGGER);
	blue_goal_trigger_settings.mIsSensor = true;
	BodyID blueGoalTriggerId = bi.CreateAndAddBody(blue_goal_trigger_settings, EActivation::DontActivate);


	// RED GOAL TRIGGER
	BoxShapeSettings red_goal_trigger_shape_settings(Vec3(1.515f, 0.745f, 0.25f));
	red_goal_trigger_shape_settings.SetEmbedded();
	ShapeSettings::ShapeResult red_goal_trigger_shape_result = red_goal_trigger_shape_settings.Create();
	ShapeRefC red_goal_trigger_shape = red_goal_trigger_shape_result.Get();
	BodyCreationSettings red_goal_trigger_settings(red_goal_trigger_shape, RVec3(0.0_r, 0.745_r, -34.0_r), Quat::sIdentity(), EMotionType::Static, Layers::GOAL_TRIGGER);
	red_goal_trigger_settings.mIsSensor = true;
	BodyID redGoalTriggerId = bi.CreateAndAddBody(red_goal_trigger_settings, EActivation::DontActivate);

	return { iceId, leftWallId, rightWallId, farWallId, nearWallId, blueGoalTriggerId, redGoalTriggerId }; 
}
void DestroyRink(BodyInterface& bi, Rink& rink)
{
	bi.RemoveBody(rink.iceId);
	bi.RemoveBody(rink.leftWallId);
	bi.RemoveBody(rink.rightWallId);
	bi.RemoveBody(rink.farWallId);
	bi.RemoveBody(rink.nearWallId);
	bi.RemoveBody(rink.blueGoalTriggerId);
	bi.RemoveBody(rink.redGoalTriggerId);

	bi.DestroyBody(rink.iceId);
	bi.DestroyBody(rink.leftWallId);
	bi.DestroyBody(rink.rightWallId);
	bi.DestroyBody(rink.farWallId);
	bi.DestroyBody(rink.nearWallId);
	bi.DestroyBody(rink.blueGoalTriggerId);
	bi.DestroyBody(rink.redGoalTriggerId);
}
