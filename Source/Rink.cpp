#include "Rink.h"
#include "Layers.h"
#include "MeshData.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;

Rink CreateRink(BodyInterface& bi)
{
	// ICE
	BoxShapeSettings ice_shape_settings(Vec3(20.0f, 0.0125f, 39.0f));
	ice_shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked as such to prevent it from being freed when its reference count goes to 0.
	ShapeSettings::ShapeResult ice_shape_result = ice_shape_settings.Create();
	ShapeRefC ice_shape = ice_shape_result.Get();
	BodyCreationSettings ice_settings(ice_shape, RVec3(0.0_r, 0.0125_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::ICE);
	ice_settings.mFriction = 0.0f;
	ice_settings.mRestitution = 0.0f;
	BodyID iceId = bi.CreateAndAddBody(ice_settings, EActivation::DontActivate);

	// Boards
	VertexList barrier_verts;
	barrier_verts.reserve(BARRIER_COLLIDER_NUM_VERTICES);
	for (int i = 0; i < BARRIER_COLLIDER_NUM_VERTICES; i++) {
		float x = BARRIER_COLLIDER_VERTICES[i].x * 1.25f;
		float y = -BARRIER_COLLIDER_VERTICES[i].z;
		float z = BARRIER_COLLIDER_VERTICES[i].y * 1.25f;
		barrier_verts.push_back(Float3(x, y, z));
	}

	IndexedTriangleList barrier_tris;
	barrier_tris.reserve(BARRIER_COLLIDER_NUM_INDICES / 3);
	for (int i = 0; i < BARRIER_COLLIDER_NUM_INDICES; i += 3) {
		barrier_tris.push_back(IndexedTriangle(
			BARRIER_COLLIDER_INDICES[i],
			BARRIER_COLLIDER_INDICES[i + 1],
			BARRIER_COLLIDER_INDICES[i + 2]
		));
	}

	MeshShapeSettings barrier_mesh(barrier_verts, barrier_tris);
	ShapeSettings::ShapeResult barrier_result = barrier_mesh.Create();
	ShapeRefC barrier_shape = barrier_result.Get();
	BodyCreationSettings boards_settings(barrier_shape, RVec3::sZero(), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	boards_settings.mFriction = 0.0f;
	boards_settings.mRestitution = 0.2f;
	BodyID boardsId = bi.CreateAndAddBody(boards_settings, EActivation::DontActivate);

	return { iceId, boardsId };
}

void DestroyRink(BodyInterface& bi, Rink& rink)
{
	bi.RemoveBody(rink.iceId);
	bi.RemoveBody(rink.boardsId);

	bi.DestroyBody(rink.iceId);
	bi.DestroyBody(rink.boardsId);
}
