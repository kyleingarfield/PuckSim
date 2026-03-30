#include "Layers.h"
#include "Player.h"
#include "Stick.h"
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
	auto printPuck = [&](const char* label) {
		RVec3 p = body_interface.GetPosition(puck.puckId);
		RVec3 com = body_interface.GetCenterOfMassPosition(puck.puckId);
		cout << label << " Pos=(" << p.GetX() << ", " << p.GetY() << ", " << p.GetZ()
			<< ") COM=(" << com.GetX() << ", " << com.GetY() << ", " << com.GetZ() << ")" << endl;
	};
	printPuck("After CreatePuck");
	Player attacker = CreatePlayer(body_interface, ATTACKER_PARAMS, RVec3(5.0_r, 0.5_r, 0.0_r));
	printPuck("After attacker");
	Player goalie = CreatePlayer(body_interface, GOALIE_PARAMS, RVec3(-5.0_r, 0.5_r, 0.0_r));
	printPuck("After goalie");
	Stick attackerStick = CreateStick(body_interface, ATTACKER_STICK_PARAMS, RVec3(5.0_r, 1.5_r, 0.0_r));
	printPuck("After att stick");
	Stick goalieStick = CreateStick(body_interface, GOALIE_STICK_PARAMS, RVec3(-5.0_r, 1.5_r, 0.0_r));
	printPuck("After goal stick");
	physics_system.OptimizeBroadPhase();
	printPuck("After optimize");

	// TEST: Set puck velocity towards side wall
	body_interface.SetLinearVelocity(puck.puckId, Vec3(10.0f, 0.0f, 0.0f));

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

		RVec3 attPos = body_interface.GetPosition(attacker.bodyId);
		UpdateStick(body_interface, attackerStick,
			Vec3(attPos) + Vec3(0, 1.5f, -1.0f),
			Vec3(attPos) + Vec3(0, 1.5f, 1.3f),
			attacker.bodyId, cDeltaTime
		);

		RVec3 goaliePos = body_interface.GetPosition(goalie.bodyId);
		UpdateStick(body_interface, goalieStick,
			Vec3(goaliePos) + Vec3(0, 1.5f, -1.0f),
			Vec3(goaliePos) + Vec3(0, 1.5f, 1.3f),
			goalie.bodyId, cDeltaTime
		);

		if (step <= 20 || step % 50 == 0) {
			RVec3 puck_pos = body_interface.GetCenterOfMassPosition(puck.puckId);
			Vec3 puck_vel = body_interface.GetLinearVelocity(puck.puckId);
			cout << "Step " << step
				<< "  Pos=(" << puck_pos.GetX() << ", " << puck_pos.GetY() << ", " << puck_pos.GetZ()
				<< ") Vel=(" << puck_vel.GetX() << ", " << puck_vel.GetY() << ", " << puck_vel.GetZ() << ")" << endl;
		}

		physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);
	}

	DestroyRink(body_interface, rink);
	DestroyPuck(body_interface, puck);
	DestroyStick(body_interface, attackerStick);
	DestroyStick(body_interface, goalieStick);
	DestroyPlayer(body_interface, attacker);
	DestroyPlayer(body_interface, goalie);

	ShutdownJolt();
	return 0;
}