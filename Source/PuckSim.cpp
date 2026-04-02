#include "Layers.h"
#include "Player.h"
#include "Stick.h"
#include "Rink.h"
#include "Goal.h"
#include "Puck.h"
#include "Listeners.h"
#include "JoltSetup.h"
#include "Scenarios.h"
#include "Scenarios.cpp"
#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <iostream>
#include <string>
#include <thread>

JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;
using namespace std;

namespace {
	int RunInteractiveDemo()
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
		Goal blueGoal = CreateGoal(body_interface, true);
		Goal redGoal = CreateGoal(body_interface, false);
		Puck puck = CreatePuck(body_interface, RVec3(0.0_r, 0.2_r, 0.0_r));
		Player attacker = CreatePlayer(body_interface, ATTACKER_PARAMS, RVec3(5.0_r, 0.5_r, 0.0_r));
		Player goalie = CreatePlayer(body_interface, GOALIE_PARAMS, RVec3(-5.0_r, 0.5_r, 0.0_r));
		Stick attackerStick = CreateStick(body_interface, ATTACKER_STICK_PARAMS, RVec3(5.0_r, 1.5_r, 0.0_r));
		Stick goalieStick = CreateStick(body_interface, GOALIE_STICK_PARAMS, RVec3(-5.0_r, 1.5_r, 0.0_r));

		contact_listener.puckBodyId = puck.puckId;
		contact_listener.attackerStickId = attackerStick.bodyId;
		contact_listener.goalieStickId = goalieStick.bodyId;

		physics_system.OptimizeBroadPhase();

		const float cDeltaTime = 1.0f / 50.0f;
		const int cCollisionSteps = 1;

		PlayerInput attackerInput = { 0.0f, 0.0f, 0.0f };
		PlayerInput goalieInput = { 0.0f, 0.0f, 0.0f };

		uint step = 0;
		while (step < 500)
		{
			++step;

			SyncPuckTrigger(body_interface, puck);
			puck.isTouchingStick = false;

			UpdatePlayerHover(body_interface, physics_system, attacker, cDeltaTime);
			UpdatePlayerHover(body_interface, physics_system, goalie, cDeltaTime);
			UpdateKeepUpright(body_interface, attacker, cDeltaTime);
			UpdateKeepUpright(body_interface, goalie, cDeltaTime);
			UpdateLaterality(body_interface, attacker, attackerInput, cDeltaTime);
			UpdateLaterality(body_interface, goalie, goalieInput, cDeltaTime);
			UpdateMovement(body_interface, attacker, attackerInput, cDeltaTime);
			UpdateMovement(body_interface, goalie, goalieInput, cDeltaTime);
			UpdateSkate(body_interface, attacker, cDeltaTime);
			UpdateSkate(body_interface, goalie, cDeltaTime);
			UpdateVelocityLean(body_interface, attacker, cDeltaTime);
			UpdateVelocityLean(body_interface, goalie, cDeltaTime);
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

			physics_system.Update(cDeltaTime, cCollisionSteps, &temp_allocator, &job_system);

			for (BodyID id : contact_listener.netDampingQueue) {
				Vec3 vel = body_interface.GetLinearVelocity(id);
				vel *= 0.75f;
				if (vel.Length() > 2.0f)
					vel = vel.Normalized() * 2.0f;
				body_interface.SetLinearVelocity(id, vel);

				Vec3 angVel = body_interface.GetAngularVelocity(id);
				angVel *= 0.75f;
				if (angVel.Length() > 2.0f)
					angVel = angVel.Normalized() * 2.0f;
				body_interface.SetAngularVelocity(id, angVel);
			}
			contact_listener.netDampingQueue.clear();

			for (BodyID id : contact_listener.stickExitQueue) {
				UpdatePuckPostStickExit(body_interface, puck, cDeltaTime);
			}
			contact_listener.stickExitQueue.clear();

			puck.isTouchingStick = !contact_listener.puckStickContactQueue.empty();
			contact_listener.puckStickContactQueue.clear();
			UpdatePuckTensor(physics_system, puck);
			UpdatePuckGroundCheck(body_interface, physics_system, puck);
		}

		DestroyRink(body_interface, rink);
		DestroyGoal(body_interface, blueGoal);
		DestroyGoal(body_interface, redGoal);
		DestroyPuck(body_interface, puck);
		DestroyStick(body_interface, attackerStick);
		DestroyStick(body_interface, goalieStick);
		DestroyPlayer(body_interface, attacker);
		DestroyPlayer(body_interface, goalie);

		ShutdownJolt();
		return 0;
	}
}

int main(int argc, char** argv)
{
	if (argc >= 3 && string(argv[1]) == "--test") {
		return RunReplayScenarios(argv[2]);
	}

	if (argc >= 2 && string(argv[1]) == "--test") {
		cerr << "Usage: PuckSim.exe --test <scenario_name|all>" << endl;
		return 1;
	}

	return RunInteractiveDemo();
}
