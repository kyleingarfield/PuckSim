#include "Goal.h"
#include "Layers.h"
#include "MeshData.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>

JPH_SUPPRESS_WARNINGS

using namespace JPH::literals;

// Goal child transform: 90 deg X rotation + parent scale (0.9, 0.9, 0.8)
// Unity left-handed 90 deg X: Y -> -Z, Z -> Y
// Combined: (mesh_x * 0.9, -mesh_z * 0.9, mesh_y * 0.8)
static Vec3 GoalTransformPoint(float mx, float my, float mz)
{
	return Vec3(mx * 0.9f, -mz * 0.9f, my * 0.8f);
}

Goal CreateGoal(BodyInterface& bi, bool isBlue)
{
	// Goal container: Red at (0,0,-34.1) identity, Blue at (0,0,34.1) with 180 deg Y rotation
	RVec3 goalPos = isBlue ? RVec3(0, 0, 34.1_r) : RVec3(0, 0, -34.1_r);
	Quat goalRot = isBlue ? Quat::sRotation(Vec3::sAxisY(), JPH_PI) : Quat::sIdentity();

	// --- POSTS: 3 capsules in a compound ---
	// All capsule values are Unity local space, transformed through GoalTransformPoint.
	// Jolt CapsuleShape runs along Y by default.
	StaticCompoundShapeSettings posts_compound;

	// Crossbar: Unity dir X, height 3.11, radius 0.06, center (0, 1.15, -1.56)
	// After transform: center (0, 1.404, 0.92), still runs along X
	// Jolt capsule halfHeight = (3.11/2 - 0.06) * 0.9 = 1.3455, radius = 0.06 * 0.9 = 0.054
	// Rotate 90 deg around Z to orient capsule from Y-axis to X-axis
	posts_compound.AddShape(
		GoalTransformPoint(0, 1.15f, -1.56f),
		Quat::sRotation(Vec3::sAxisZ(), 0.5f * JPH_PI),
		new CapsuleShape(1.3455f, 0.054f)
	);

	// Right post: Unity dir Z, height 2.0, radius 0.06, center (1.5, 1.15, -0.62)
	// After transform: center (1.35, 0.558, 0.92), runs along Y (Z rotated to Y by 90 deg X)
	// Jolt capsule halfHeight = (2.0/2 - 0.06) * 0.9 = 0.846, radius = 0.06 * 0.9 = 0.054
	posts_compound.AddShape(
		GoalTransformPoint(1.5f, 1.15f, -0.62f),
		Quat::sIdentity(),
		new CapsuleShape(0.846f, 0.054f)
	);

	// Left post: same as right but mirrored X
	posts_compound.AddShape(
		GoalTransformPoint(-1.5f, 1.15f, -0.62f),
		Quat::sIdentity(),
		new CapsuleShape(0.846f, 0.054f)
	);

	ShapeRefC posts_shape = posts_compound.Create().Get();
	BodyCreationSettings posts_settings(posts_shape, goalPos, goalRot, EMotionType::Static, Layers::GOAL_POST);
	posts_settings.mFriction = 0.0f;
	posts_settings.mRestitution = 0.0f;
	BodyID postsId = bi.CreateAndAddBody(posts_settings, EActivation::DontActivate);

	// --- NET COLLIDER: non-convex MeshShape ---
	VertexList net_verts;
	net_verts.reserve(GOAL_NET_COLLIDER_NUM_VERTICES);
	for (int i = 0; i < GOAL_NET_COLLIDER_NUM_VERTICES; i++) {
		Vec3 p = GoalTransformPoint(
			GOAL_NET_COLLIDER_VERTICES[i].x,
			GOAL_NET_COLLIDER_VERTICES[i].y,
			GOAL_NET_COLLIDER_VERTICES[i].z);
		net_verts.push_back(Float3(p.GetX(), p.GetY(), p.GetZ()));
	}

	IndexedTriangleList net_tris;
	net_tris.reserve(GOAL_NET_COLLIDER_NUM_INDICES / 3);
	for (int i = 0; i < GOAL_NET_COLLIDER_NUM_INDICES; i += 3) {
		net_tris.push_back(IndexedTriangle(
			GOAL_NET_COLLIDER_INDICES[i],
			GOAL_NET_COLLIDER_INDICES[i + 1],
			GOAL_NET_COLLIDER_INDICES[i + 2]
		));
	}

	ShapeRefC net_shape = MeshShapeSettings(net_verts, net_tris).Create().Get();
	BodyCreationSettings net_settings(net_shape, goalPos, goalRot, EMotionType::Static, Layers::GOAL_NET);
	net_settings.mFriction = 0.0f;
	net_settings.mRestitution = 0.0f;
	BodyID netColliderId = bi.CreateAndAddBody(net_settings, EActivation::DontActivate);

	// --- GOAL TRIGGER: convex hull sensor ---
	Array<Vec3> trigger_points;
	trigger_points.reserve(GOAL_TRIGGER_NUM_VERTICES);
	for (int i = 0; i < GOAL_TRIGGER_NUM_VERTICES; i++)
		trigger_points.push_back(GoalTransformPoint(
			GOAL_TRIGGER_VERTICES[i].x,
			GOAL_TRIGGER_VERTICES[i].y,
			GOAL_TRIGGER_VERTICES[i].z));

	ShapeRefC trigger_shape = ConvexHullShapeSettings(trigger_points).Create().Get();
	BodyCreationSettings trigger_settings(trigger_shape, goalPos, goalRot, EMotionType::Static, Layers::GOAL_TRIGGER);
	trigger_settings.mIsSensor = true;
	BodyID triggerId = bi.CreateAndAddBody(trigger_settings, EActivation::DontActivate);

	// --- GOAL PLAYER COLLIDER: convex hull ---
	Array<Vec3> player_col_points;
	player_col_points.reserve(GOAL_PLAYER_COLLIDER_NUM_VERTICES);
	for (int i = 0; i < GOAL_PLAYER_COLLIDER_NUM_VERTICES; i++)
		player_col_points.push_back(GoalTransformPoint(
			GOAL_PLAYER_COLLIDER_VERTICES[i].x,
			GOAL_PLAYER_COLLIDER_VERTICES[i].y,
			GOAL_PLAYER_COLLIDER_VERTICES[i].z));

	ShapeRefC player_col_shape = ConvexHullShapeSettings(player_col_points).Create().Get();
	BodyCreationSettings player_col_settings(player_col_shape, goalPos, goalRot, EMotionType::Static, Layers::PLAYER_COLLIDER);
	player_col_settings.mFriction = 0.0f;
	player_col_settings.mRestitution = 0.0f;
	BodyID playerColliderId = bi.CreateAndAddBody(player_col_settings, EActivation::DontActivate);

	return { postsId, netColliderId, triggerId, playerColliderId };
}

void DestroyGoal(BodyInterface& bi, Goal& goal)
{
	bi.RemoveBody(goal.postsId);
	bi.RemoveBody(goal.netColliderId);
	bi.RemoveBody(goal.triggerId);
	bi.RemoveBody(goal.playerColliderId);

	bi.DestroyBody(goal.postsId);
	bi.DestroyBody(goal.netColliderId);
	bi.DestroyBody(goal.triggerId);
	bi.DestroyBody(goal.playerColliderId);
}
