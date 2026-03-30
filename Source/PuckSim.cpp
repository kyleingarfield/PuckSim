#include "Layers.h"
#include "Player.h"
#include "Rink.h"
#include "Puck.h"
#include "Listeners.h"
#include "JoltSetup.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <iostream>
#include <thread>

JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;
using namespace std;


// Program entry point
int main(int argc, char** argv)
{
	InitJolt();
	TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
	JobSystemThreadPool job_system(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);
	const uint cMaxBodies = 1024;
	const uint cNumBodyMutexes = 0;
	const uint cMaxBodyPairs = 1024;
	const uint cMaxContactConstraints = 1024;

	BroadPhaseLayerInterfaceTable broad_phase_layer_interface(Layers::NUM_LAYERS, BroadPhaseLayers::NUM_LAYERS);
	SetupBroadPhaseMapping(broad_phase_layer_interface);

	ObjectLayerPairFilterTable object_vs_object_layer_filter(Layers::NUM_LAYERS);
	SetupCollisionPairs(object_vs_object_layer_filter);


	ObjectVsBroadPhaseLayerFilterTable object_vs_broadphase_layer_filter(
		broad_phase_layer_interface, BroadPhaseLayers::NUM_LAYERS,
		object_vs_object_layer_filter, Layers::NUM_LAYERS);

	PhysicsSystem physics_system;
	physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);


	MyBodyActivationListener body_activation_listener;
	physics_system.SetBodyActivationListener(&body_activation_listener);

	MyContactListener contact_listener;
	physics_system.SetContactListener(&contact_listener);

	BodyInterface& body_interface = physics_system.GetBodyInterface();


	Rink rink = CreateRink(body_interface);
	Puck puck = CreatePuck(body_interface, RVec3(0.0_r, 0.2_r, 0.0_r));
	Player attacker = CreatePlayer(body_interface, ATTACKER_PARAMS, RVec3(5.0_r, 0.5_r, 0.0_r));
	Player goalie = CreatePlayer(body_interface, GOALIE_PARAMS, RVec3(-5.0_r, 0.5_r, 0.0_r));

	// TEST: Set puck velocity towards blue goal
	body_interface.SetLinearVelocity(puck.puckId, Vec3(10.0f, 3.0f, 0.0f));

	const float cDeltaTime = 1.0f / 50.0f;
	const int cCollisionSteps = 1;

	physics_system.OptimizeBroadPhase();

	uint step = 0;
	while (step < 500)
	{
		++step;

		SyncPuckTrigger(body_interface, puck);
		UpdatePlayerHover(body_interface, physics_system, attacker, cDeltaTime);
		UpdatePlayerHover(body_interface, physics_system, goalie, cDeltaTime);
		SyncPlayerMesh(body_interface, attacker, cDeltaTime);
		SyncPlayerMesh(body_interface, goalie, cDeltaTime); 

		if (step <= 20) {
			RVec3 puck_pos = body_interface.GetCenterOfMassPosition(puck.puckId);
			Vec3 puck_vel = body_interface.GetLinearVelocity(puck.puckId);
			cout << "Step " << step << endl;
			cout << "  Puck Pos=(" << puck_pos.GetX() << ", " << puck_pos.GetY() << ", " <<
				puck_pos.GetZ()
				<< ") Vel=(" << puck_vel.GetX() << ", " << puck_vel.GetY() << ", " << puck_vel.GetZ() <<
				")" << endl;

			RVec3 att_pos = body_interface.GetPosition(attacker.bodyId);
			RVec3 goal_pos = body_interface.GetPosition(goalie.bodyId);
			cout << "  Attacker Pos=(" << att_pos.GetX() << ", " << att_pos.GetY() << ", " <<
				att_pos.GetZ() << ")" << endl;
			cout << "  Goalie   Pos=(" << goal_pos.GetX() << ", " << goal_pos.GetY() << ", " <<
				goal_pos.GetZ() << ")" << endl;
		}

		physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);
	}
	DestroyRink(body_interface, rink);
	DestroyPuck(body_interface, puck);
	DestroyPlayer(body_interface, attacker);
	DestroyPlayer(body_interface, goalie);

	ShutdownJolt();
	return 0;
}