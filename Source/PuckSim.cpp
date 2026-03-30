// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2025 Jorrit Rouwe
// SPDX-License-Identifier: CC0-1.0
// This file is in the public domain. It serves as an example to start building your own application using Jolt Physics. Feel free to copy paste without attribution!

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>

// STL includes
#include <iostream>
#include <cstdarg>
#include <thread>

// Disable common warnings triggered by Jolt, you can use JPH_SUPPRESS_WARNING_PUSH / JPH_SUPPRESS_WARNING_POP to store and restore the warning state
JPH_SUPPRESS_WARNINGS

// All Jolt symbols are in the JPH namespace
using namespace JPH;

// If you want your code to compile using single or double precision write 0.0_r to get a Real value that compiles to double or float depending if JPH_DOUBLE_PRECISION is set or not.
using namespace JPH::literals;

// We're also using STL classes in this example
using namespace std;

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char* inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print to the TTY
	cout << buffer << endl;
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
{
	// Print to the TTY
	cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << endl;

	// Breakpoint
	return true;
};

#endif // JPH_ENABLE_ASSERTS

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics simulation
// but only if you do collision testing).
namespace Layers
{
	static constexpr ObjectLayer DEFAULT = 0;
	static constexpr ObjectLayer TRANSPARENT_FX = 1;
	static constexpr ObjectLayer IGNORE_RAYCAST = 2;
	static constexpr ObjectLayer LEVEL = 3;
	static constexpr ObjectLayer WATER = 4;
	static constexpr ObjectLayer UI = 5;
	static constexpr ObjectLayer STICK = 6;
	static constexpr ObjectLayer PUCK = 7;
	static constexpr ObjectLayer PLAYER = 8;
	static constexpr ObjectLayer PLAYER_MESH = 9;
	static constexpr ObjectLayer POST_PROCESSING = 10;
	static constexpr ObjectLayer GOAL_POST = 11;
	static constexpr ObjectLayer BOARDS = 12;
	static constexpr ObjectLayer ICE = 13;
	static constexpr ObjectLayer GOAL_NET = 14;
	static constexpr ObjectLayer GOAL_TRIGGER = 15;
	static constexpr ObjectLayer PLAYER_COLLIDER = 16;
	static constexpr ObjectLayer NET_CLOTH_COLLIDER = 17;
	static constexpr ObjectLayer GOAL_FRAME = 18;
	static constexpr ObjectLayer PUCK_TRIGGER = 19;
	static constexpr ObjectLayer NUM_LAYERS = 20;
};


// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers
{
	static constexpr BroadPhaseLayer STATIC(0);
	static constexpr BroadPhaseLayer DYNAMIC(1);
	static constexpr BroadPhaseLayer SENSOR(2);
	static constexpr uint NUM_LAYERS(3);
};



// An example contact listener
class MyContactListener : public ContactListener
{
public:
	// See: ContactListener
	virtual ValidateResult	OnContactValidate(const Body& inBody1, const Body& inBody2, RVec3Arg inBaseOffset, const CollideShapeResult& inCollisionResult) override
	{

		// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
		return ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	virtual void			OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
	{
	}

	virtual void			OnContactPersisted(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
	{
	}

	virtual void			OnContactRemoved(const SubShapeIDPair& inSubShapePair) override
	{
	}
};

// An example activation listener
class MyBodyActivationListener : public BodyActivationListener
{
public:
	virtual void		OnBodyActivated(const BodyID& inBodyID, uint64 inBodyUserData) override
	{
	}

	virtual void		OnBodyDeactivated(const BodyID& inBodyID, uint64 inBodyUserData) override
	{
	}
};

// Program entry point
int main(int argc, char** argv)
{
	// Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
	// This needs to be done before any other Jolt function is called.
	RegisterDefaultAllocator();

	// Install trace and assert callbacks
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

		// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
		// It is not directly used in this example but still required.
		Factory::sInstance = new Factory();

	// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
	// If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
	// If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
	RegisterTypes();

	// We need a temp allocator for temporary allocations during the physics update. We're
	// pre-allocating 10 MB to avoid having to do allocations during the physics update.
	// B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
	// If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
	// malloc / free.
	TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

	// We need a job system that will execute physics jobs on multiple threads. Typically
	// you would implement the JobSystem interface yourself and let Jolt Physics run on top
	// of your own job scheduler. JobSystemThreadPool is an example implementation.
	JobSystemThreadPool job_system(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

	// This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const uint cMaxBodies = 1024;

	// This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
	const uint cNumBodyMutexes = 0;

	// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
	// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
	// too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
	const uint cMaxBodyPairs = 1024;

	// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
	// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
	// Note: This value is low because this is a simple test. For a real project use something in the order of 10240.
	const uint cMaxContactConstraints = 1024;

	// Create mapping table from object layer to broadphase layer
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	// Also have a look at BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler interface.
	BroadPhaseLayerInterfaceTable broad_phase_layer_interface(Layers::NUM_LAYERS, BroadPhaseLayers::NUM_LAYERS);

	// Static geometry
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::ICE, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::BOARDS, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::GOAL_POST, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::GOAL_FRAME, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::GOAL_NET, BroadPhaseLayers::STATIC);

	// Dynamic objects
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PUCK, BroadPhaseLayers::DYNAMIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PLAYER, BroadPhaseLayers::DYNAMIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PLAYER_MESH, BroadPhaseLayers::DYNAMIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::STICK, BroadPhaseLayers::DYNAMIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PLAYER_COLLIDER, BroadPhaseLayers::DYNAMIC);

	// Sensors
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::GOAL_TRIGGER, BroadPhaseLayers::SENSOR);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::PUCK_TRIGGER, BroadPhaseLayers::SENSOR);

	// Unused layers (map to static — they have no collision pairs anyway)
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::DEFAULT, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::TRANSPARENT_FX, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::IGNORE_RAYCAST, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::LEVEL, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::WATER, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::UI, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::POST_PROCESSING, BroadPhaseLayers::STATIC);
	broad_phase_layer_interface.MapObjectToBroadPhaseLayer(Layers::NET_CLOTH_COLLIDER, BroadPhaseLayers::STATIC);


	// Create class that filters object vs broadphase layers
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	// Also have a look at ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for a simpler interface.
	ObjectLayerPairFilterTable object_vs_object_layer_filter(Layers::NUM_LAYERS);

	// Stick collisions
	object_vs_object_layer_filter.EnableCollision(Layers::STICK, Layers::STICK);
	object_vs_object_layer_filter.EnableCollision(Layers::STICK, Layers::PUCK);

	// Puck collisions
	object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::PUCK);
	object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::PLAYER_MESH);
	object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::GOAL_POST);
	object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::BOARDS);
	object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::ICE);
	object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::GOAL_NET);
	object_vs_object_layer_filter.EnableCollision(Layers::PUCK, Layers::GOAL_FRAME);

	// Player collisions
	object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::PLAYER);
	object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::GOAL_POST);
	object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::BOARDS);
	object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::ICE);
	object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::GOAL_NET);
	object_vs_object_layer_filter.EnableCollision(Layers::PLAYER, Layers::PLAYER_COLLIDER);

	// Goal detection
	object_vs_object_layer_filter.EnableCollision(Layers::GOAL_TRIGGER, Layers::PUCK_TRIGGER);

	// Create class that filters object vs object layers
	// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
	// Also have a look at ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler interface.
	ObjectVsBroadPhaseLayerFilterTable object_vs_broadphase_layer_filter(
		broad_phase_layer_interface, BroadPhaseLayers::NUM_LAYERS,
		object_vs_object_layer_filter, Layers::NUM_LAYERS);

	// Now we can create the actual physics system.
	PhysicsSystem physics_system;
	physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

	// A body activation listener gets notified when bodies activate and go to sleep
	// Note that this is called from a job so whatever you do here needs to be thread safe.
	// Registering one is entirely optional.
	MyBodyActivationListener body_activation_listener;
	physics_system.SetBodyActivationListener(&body_activation_listener);

	// A contact listener gets notified when bodies (are about to) collide, and when they separate again.
	// Note that this is called from a job so whatever you do here needs to be thread safe.
	// Registering one is entirely optional.
	MyContactListener contact_listener;
	physics_system.SetContactListener(&contact_listener);

	// The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
	// variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
	BodyInterface& body_interface = physics_system.GetBodyInterface();

	// Next we can create a rigid body to serve as the floor, we make a large box
	// Create the settings for the collision volume (the shape).
	// Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
	BoxShapeSettings ice_shape_settings(Vec3(15.0f, 0.0125f, 30.5f));
	ice_shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked as such to prevent it from being freed when its reference count goes to 0.

	// Create the shape
	ShapeSettings::ShapeResult ice_shape_result = ice_shape_settings.Create();
	ShapeRefC ice_shape = ice_shape_result.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

	// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
	BodyCreationSettings ice_settings(ice_shape, RVec3(0.0_r, 0.0125_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::ICE);
	ice_settings.mFriction = 0.0f;
	ice_settings.mRestitution = 0.0f;

	// Create the actual rigid body
	Body* ice = body_interface.CreateBody(ice_settings); // Note that if we run out of bodies this can return nullptr

	// Add it to the world
	body_interface.AddBody(ice->GetID(), EActivation::DontActivate);

	// LEFT WALL
	BoxShapeSettings left_wall_shape_settings(Vec3(0.4f, 12.5f, 30.5f));
	left_wall_shape_settings.SetEmbedded();

	ShapeSettings::ShapeResult left_wall_shape_result = left_wall_shape_settings.Create();
	ShapeRefC left_wall_shape = left_wall_shape_result.Get(); 

	BodyCreationSettings left_wall_settings(left_wall_shape, RVec3(-15.4_r, 12.5_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	left_wall_settings.mFriction = 0.0f;
	left_wall_settings.mRestitution = 0.2f;

	Body* left_wall = body_interface.CreateBody(left_wall_settings);

	body_interface.AddBody(left_wall->GetID(), EActivation::DontActivate);

	// RIGHT WALL
	BoxShapeSettings right_wall_shape_settings(Vec3(0.4f, 12.5f, 30.5f));
	right_wall_shape_settings.SetEmbedded();

	ShapeSettings::ShapeResult right_wall_shape_result = right_wall_shape_settings.Create();
	ShapeRefC right_wall_shape = right_wall_shape_result.Get();

	BodyCreationSettings right_wall_settings(right_wall_shape, RVec3(15.4_r, 12.5_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	right_wall_settings.mFriction = 0.0f;
	right_wall_settings.mRestitution = 0.2f;

	Body* right_wall = body_interface.CreateBody(right_wall_settings);

	body_interface.AddBody(right_wall->GetID(), EActivation::DontActivate);

	// FAR END WALL
	BoxShapeSettings far_wall_shape_settings(Vec3(15.4f, 12.5f, 0.4f));
	far_wall_shape_settings.SetEmbedded();

	ShapeSettings::ShapeResult far_wall_shape_result = far_wall_shape_settings.Create();
	ShapeRefC far_wall_shape = far_wall_shape_result.Get();

	BodyCreationSettings far_wall_settings(far_wall_shape, RVec3(0.0_r, 12.5_r, 31.3_r), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	far_wall_settings.mFriction = 0.0f;
	far_wall_settings.mRestitution = 0.2f;

	Body* far_wall = body_interface.CreateBody(far_wall_settings);

	body_interface.AddBody(far_wall->GetID(), EActivation::DontActivate);

	// NEAR END WALL
	BoxShapeSettings near_wall_shape_settings(Vec3(15.4f, 12.5f, 0.4f));
	near_wall_shape_settings.SetEmbedded();

	ShapeSettings::ShapeResult near_wall_shape_result = near_wall_shape_settings.Create();
	ShapeRefC near_wall_shape = near_wall_shape_result.Get();

	BodyCreationSettings near_wall_settings(near_wall_shape, RVec3(0.0_r, 12.5_r, -31.3_r), Quat::sIdentity(), EMotionType::Static, Layers::BOARDS);
	near_wall_settings.mFriction = 0.0f;
	near_wall_settings.mRestitution = 0.2f;

	Body* near_wall = body_interface.CreateBody(near_wall_settings);

	body_interface.AddBody(near_wall->GetID(), EActivation::DontActivate);



	// Now create a dynamic body to bounce on the floor
	// Note that this uses the shorthand version of creating and adding a body to the world
	BodyCreationSettings puck_settings(new SphereShape(0.152f), RVec3(0.0_r, 0.2_r, 0.0_r), Quat::sIdentity(), EMotionType::Dynamic, Layers::PUCK);
	puck_settings.mFriction = 0.0f;
	puck_settings.mRestitution = 0.0f;
	puck_settings.mLinearDamping = 0.3f;
	puck_settings.mAngularDamping = 0.3f;
	puck_settings.mMaxLinearVelocity = 30.f;
	puck_settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
	puck_settings.mMassPropertiesOverride.mMass = 0.375f;

	BodyID puck_id = body_interface.CreateAndAddBody(puck_settings, EActivation::Activate);

	// Now you can interact with the dynamic body, in this case we're going to give it a velocity.
	// (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
	body_interface.SetLinearVelocity(puck_id, Vec3(10.0f, 0.0f, 5.0f));

	// We simulate the physics world in discrete time steps. 60 Hz is a good rate to update the physics system.
	const float cDeltaTime = 1.0f / 50.0f;

	// Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
	// You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
	// Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
	physics_system.OptimizeBroadPhase();

	// Now we're ready to simulate the body, keep simulating until it goes to sleep
	uint step = 0;
	while (body_interface.IsActive(puck_id))
	{
		// Next step
		++step;

		// Output current position and velocity of the sphere
		if (step % 50 == 0) {
			RVec3 position = body_interface.GetCenterOfMassPosition(puck_id);
			Vec3 velocity = body_interface.GetLinearVelocity(puck_id);
			cout << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << endl;
		}

		// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
		const int cCollisionSteps = 1;

		// Step the world
		physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);
	}

	// Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added at any time.
	body_interface.RemoveBody(puck_id);

	// Destroy the sphere. After this the sphere ID is no longer valid.
	body_interface.DestroyBody(puck_id);

	// Remove and destroy the floor
	body_interface.RemoveBody(ice->GetID());
	body_interface.DestroyBody(ice->GetID());

	// Unregisters all types with the factory and cleans up the default material
	UnregisterTypes();

	// Destroy the factory
	delete Factory::sInstance;
	Factory::sInstance = nullptr;

	return 0;
}