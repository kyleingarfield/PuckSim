#include "Scenarios.h"

#include "Goal.h"
#include "JoltSetup.h"
#include "Layers.h"
#include "Listeners.h"
#include "Player.h"
#include "Puck.h"
#include "Rink.h"
#include "Stick.h"
#include "Telemetry.h"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/PhysicsSystem.h>

#ifdef _MSC_VER
#pragma warning(disable : 4530)
#endif

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace JPH;
using namespace JPH::literals;

namespace {
	constexpr float kDeltaTime = 1.0f / 50.0f;
	constexpr int kCollisionSteps = 1;
	constexpr uint kMaxBodies = 1024;
	constexpr uint kNumBodyMutexes = 0;
	constexpr uint kMaxBodyPairs = 1024;
	constexpr uint kMaxContactConstraints = 1024;
	const char* kOutputDirectory = R"(C:\Users\kyle\Developer\PuckMods\Sim2GameTest\recordings)";

	enum class ReplayScenarioId : uint8_t {
		PuckFreeSlide = 0,
		PuckWallBounce = 1,
		PuckStickExit = 2,
		PlayerForward = 3,
		PlayerTurn = 4,
		PlayerLateral = 5,
		PlayerHover = 6,
		StickTracking = 7,
		FullPlay = 8,
	};

	const char* ToScenarioName(ReplayScenarioId scenarioId)
	{
		switch (scenarioId) {
		case ReplayScenarioId::PuckFreeSlide: return "puck_free_slide";
		case ReplayScenarioId::PuckWallBounce: return "puck_wall_bounce";
		case ReplayScenarioId::PuckStickExit: return "puck_stick_exit";
		case ReplayScenarioId::PlayerForward: return "player_forward";
		case ReplayScenarioId::PlayerTurn: return "player_turn";
		case ReplayScenarioId::PlayerLateral: return "player_lateral";
		case ReplayScenarioId::PlayerHover: return "player_hover";
		case ReplayScenarioId::StickTracking: return "stick_tracking";
		case ReplayScenarioId::FullPlay: return "full_play";
		default: return "unknown";
		}
	}

	bool TryParseScenario(const std::string& requested, ReplayScenarioId& scenarioId)
	{
		if (requested == "puck_free_slide") { scenarioId = ReplayScenarioId::PuckFreeSlide; return true; }
		if (requested == "puck_wall_bounce") { scenarioId = ReplayScenarioId::PuckWallBounce; return true; }
		if (requested == "player_forward") { scenarioId = ReplayScenarioId::PlayerForward; return true; }
		if (requested == "player_turn") { scenarioId = ReplayScenarioId::PlayerTurn; return true; }
		if (requested == "player_lateral") { scenarioId = ReplayScenarioId::PlayerLateral; return true; }
		if (requested == "player_hover") { scenarioId = ReplayScenarioId::PlayerHover; return true; }
		return false;
	}

	std::string MakeOutputPath(ReplayScenarioId scenarioId)
	{
		return std::string(kOutputDirectory) + "\\" + ToScenarioName(scenarioId) + "_sim.bin";
	}

	struct ScenarioWorld {
		TempAllocatorImpl tempAllocator;
		JobSystemSingleThreaded jobSystem;
		std::unique_ptr<BroadPhaseLayerInterfaceTable> broadPhaseLayerInterface;
		std::unique_ptr<ObjectLayerPairFilterTable> objectVsObjectLayerFilter;
		std::unique_ptr<ObjectVsBroadPhaseLayerFilterTable> objectVsBroadphaseLayerFilter;
		PhysicsSystem physicsSystem;
		MyBodyActivationListener bodyActivationListener;
		MyContactListener contactListener;
		BodyInterface* bodyInterface = nullptr;
		Rink rink{};
		Goal blueGoal{};
		Goal redGoal{};
		Puck puck{};
		Player attacker{};
		Player goalie{};
		Stick attackerStick{};
		Stick goalieStick{};
		bool hasPuck = false;
		bool hasAttacker = false;
		bool hasGoalie = false;
		bool hasAttackerStick = false;
		bool hasGoalieStick = false;

		ScenarioWorld()
			: tempAllocator(10 * 1024 * 1024)
			, jobSystem(cMaxPhysicsJobs)
		{
			broadPhaseLayerInterface = std::make_unique<BroadPhaseLayerInterfaceTable>(Layers::NUM_LAYERS, BroadPhaseLayers::NUM_LAYERS);
			SetupBroadPhaseMapping(*broadPhaseLayerInterface);

			objectVsObjectLayerFilter = std::make_unique<ObjectLayerPairFilterTable>(Layers::NUM_LAYERS);
			SetupCollisionPairs(*objectVsObjectLayerFilter);

			objectVsBroadphaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterTable>(
				*broadPhaseLayerInterface,
				BroadPhaseLayers::NUM_LAYERS,
				*objectVsObjectLayerFilter,
				Layers::NUM_LAYERS
			);

			physicsSystem.Init(
				kMaxBodies,
				kNumBodyMutexes,
				kMaxBodyPairs,
				kMaxContactConstraints,
				*broadPhaseLayerInterface,
				*objectVsBroadphaseLayerFilter,
				*objectVsObjectLayerFilter
			);

			physicsSystem.SetBodyActivationListener(&bodyActivationListener);
			physicsSystem.SetContactListener(&contactListener);
			bodyInterface = &physicsSystem.GetBodyInterface();

			contactListener.puckBodyId = BodyID();
			contactListener.attackerStickId = BodyID();
			contactListener.goalieStickId = BodyID();

			rink = CreateRink(*bodyInterface);
			blueGoal = CreateGoal(*bodyInterface, true);
			redGoal = CreateGoal(*bodyInterface, false);
			physicsSystem.OptimizeBroadPhase();
		}

		~ScenarioWorld()
		{
			if (hasGoalieStick) DestroyStick(*bodyInterface, goalieStick);
			if (hasAttackerStick) DestroyStick(*bodyInterface, attackerStick);
			if (hasGoalie) DestroyPlayer(*bodyInterface, goalie);
			if (hasAttacker) DestroyPlayer(*bodyInterface, attacker);
			if (hasPuck) DestroyPuck(*bodyInterface, puck);
			DestroyGoal(*bodyInterface, blueGoal);
			DestroyGoal(*bodyInterface, redGoal);
			DestroyRink(*bodyInterface, rink);
		}

		void CreateScenarioPuck(RVec3 position, Vec3 velocity, Vec3 angularVelocity = Vec3::sZero())
		{
			puck = CreatePuck(*bodyInterface, position);
			hasPuck = true;
			bodyInterface->SetLinearVelocity(puck.puckId, velocity);
			bodyInterface->SetAngularVelocity(puck.puckId, angularVelocity);
			contactListener.puckBodyId = puck.puckId;
		}

		void CreateScenarioAttacker(RVec3 position, Vec3 velocity = Vec3::sZero(), Vec3 angularVelocity = Vec3::sZero())
		{
			attacker = CreatePlayer(*bodyInterface, ATTACKER_PARAMS, position);
			hasAttacker = true;
			bodyInterface->SetLinearVelocity(attacker.bodyId, velocity);
			bodyInterface->SetAngularVelocity(attacker.bodyId, angularVelocity);
		}

		void Step(const PlayerInput& attackerInput, const PlayerInput& goalieInput)
		{
			if (hasPuck) {
				SyncPuckTrigger(*bodyInterface, puck);
				puck.isTouchingStick = false;
			}

			if (hasAttacker) {
				UpdatePlayerHover(*bodyInterface, physicsSystem, attacker, kDeltaTime);
				UpdateKeepUpright(*bodyInterface, attacker, kDeltaTime);
				UpdateLaterality(*bodyInterface, attacker, attackerInput, kDeltaTime);
				UpdateMovement(*bodyInterface, attacker, attackerInput, kDeltaTime);
				UpdateSkate(*bodyInterface, attacker, kDeltaTime);
				UpdateVelocityLean(*bodyInterface, attacker, kDeltaTime);
				SyncPlayerMesh(*bodyInterface, attacker, kDeltaTime);
			}

			if (hasGoalie) {
				UpdatePlayerHover(*bodyInterface, physicsSystem, goalie, kDeltaTime);
				UpdateKeepUpright(*bodyInterface, goalie, kDeltaTime);
				UpdateLaterality(*bodyInterface, goalie, goalieInput, kDeltaTime);
				UpdateMovement(*bodyInterface, goalie, goalieInput, kDeltaTime);
				UpdateSkate(*bodyInterface, goalie, kDeltaTime);
				UpdateVelocityLean(*bodyInterface, goalie, kDeltaTime);
				SyncPlayerMesh(*bodyInterface, goalie, kDeltaTime);
			}

			if (hasAttackerStick && hasAttacker) {
				RVec3 attackerPosition = bodyInterface->GetPosition(attacker.bodyId);
				UpdateStick(
					*bodyInterface,
					attackerStick,
					Vec3(attackerPosition) + Vec3(0, 1.5f, -1.0f),
					Vec3(attackerPosition) + Vec3(0, 1.5f, 1.3f),
					attacker.bodyId,
					kDeltaTime
				);
			}

			if (hasGoalieStick && hasGoalie) {
				RVec3 goaliePosition = bodyInterface->GetPosition(goalie.bodyId);
				UpdateStick(
					*bodyInterface,
					goalieStick,
					Vec3(goaliePosition) + Vec3(0, 1.5f, -1.0f),
					Vec3(goaliePosition) + Vec3(0, 1.5f, 1.3f),
					goalie.bodyId,
					kDeltaTime
				);
			}

			physicsSystem.Update(kDeltaTime, kCollisionSteps, &tempAllocator, &jobSystem);
			RunPostStep();
		}

		void RunPostStep()
		{
			for (BodyID id : contactListener.netDampingQueue) {
				Vec3 velocity = bodyInterface->GetLinearVelocity(id);
				velocity *= 0.75f;
				if (velocity.Length() > 2.0f) {
					velocity = velocity.Normalized() * 2.0f;
				}
				bodyInterface->SetLinearVelocity(id, velocity);

				Vec3 angularVelocity = bodyInterface->GetAngularVelocity(id);
				angularVelocity *= 0.75f;
				if (angularVelocity.Length() > 2.0f) {
					angularVelocity = angularVelocity.Normalized() * 2.0f;
				}
				bodyInterface->SetAngularVelocity(id, angularVelocity);
			}
			contactListener.netDampingQueue.clear();

			if (!hasPuck) {
				contactListener.stickExitQueue.clear();
				contactListener.puckStickContactQueue.clear();
				return;
			}

			for (BodyID id : contactListener.stickExitQueue) {
				UpdatePuckPostStickExit(*bodyInterface, puck, kDeltaTime);
			}
			contactListener.stickExitQueue.clear();

			puck.isTouchingStick = !contactListener.puckStickContactQueue.empty();
			contactListener.puckStickContactQueue.clear();
			UpdatePuckTensor(physicsSystem, puck);
			UpdatePuckGroundCheck(*bodyInterface, physicsSystem, puck);
		}
	};

	bool FinalizeScenario(ReplayScenarioId scenarioId, TelemetryWriter& writer)
	{
		std::string outputPath = MakeOutputPath(scenarioId);
		if (!writer.WriteToFile(outputPath)) {
			std::cerr << "Failed to write telemetry file: " << outputPath << std::endl;
			return false;
		}

		std::cout << "Wrote telemetry: " << outputPath << std::endl;
		return true;
	}

	bool RunPuckFreeSlideScenario()
	{
		ScenarioWorld world;
		world.CreateScenarioPuck(RVec3(0.0_r, 0.075_r, 0.0_r), Vec3(5.0f, 0.0f, 3.0f));

		TelemetryWriter writer(static_cast<uint8_t>(ReplayScenarioId::PuckFreeSlide), 1, kDeltaTime);
		PlayerInput neutralInput{ 0.0f, 0.0f, 0.0f };

		for (int frame = 0; frame < 200; ++frame) {
			world.Step(neutralInput, neutralInput);
			writer.AddFrame({ CaptureTelemetryRecord(*world.bodyInterface, world.puck.puckId) });
		}

		return FinalizeScenario(ReplayScenarioId::PuckFreeSlide, writer);
	}

	bool RunPuckWallBounceScenario()
	{
		ScenarioWorld world;
		world.CreateScenarioPuck(RVec3(15.0_r, 0.075_r, 0.0_r), Vec3(10.0f, 0.0f, 0.0f));

		TelemetryWriter writer(static_cast<uint8_t>(ReplayScenarioId::PuckWallBounce), 1, kDeltaTime);
		PlayerInput neutralInput{ 0.0f, 0.0f, 0.0f };

		for (int frame = 0; frame < 200; ++frame) {
			world.Step(neutralInput, neutralInput);
			writer.AddFrame({ CaptureTelemetryRecord(*world.bodyInterface, world.puck.puckId) });
		}

		return FinalizeScenario(ReplayScenarioId::PuckWallBounce, writer);
	}

	bool RunPlayerForwardScenario()
	{
		ScenarioWorld world;
		world.CreateScenarioAttacker(RVec3(0.0_r, 0.5_r, 0.0_r));

		TelemetryWriter writer(static_cast<uint8_t>(ReplayScenarioId::PlayerForward), 1, kDeltaTime);
		PlayerInput attackerInput{ 1.0f, 0.0f, 0.0f };
		PlayerInput neutralInput{ 0.0f, 0.0f, 0.0f };

		for (int frame = 0; frame < 500; ++frame) {
			world.Step(attackerInput, neutralInput);
			writer.AddFrame({ CaptureTelemetryRecord(*world.bodyInterface, world.attacker.bodyId) });
		}

		return FinalizeScenario(ReplayScenarioId::PlayerForward, writer);
	}

	bool RunPlayerTurnScenario()
	{
		ScenarioWorld world;
		world.CreateScenarioAttacker(RVec3(0.0_r, 0.5_r, 0.0_r));

		TelemetryWriter writer(static_cast<uint8_t>(ReplayScenarioId::PlayerTurn), 1, kDeltaTime);
		PlayerInput attackerInput{ 1.0f, 1.0f, 0.0f };
		PlayerInput neutralInput{ 0.0f, 0.0f, 0.0f };

		for (int frame = 0; frame < 300; ++frame) {
			world.Step(attackerInput, neutralInput);
			writer.AddFrame({ CaptureTelemetryRecord(*world.bodyInterface, world.attacker.bodyId) });
		}

		return FinalizeScenario(ReplayScenarioId::PlayerTurn, writer);
	}

	bool RunPlayerLateralScenario()
	{
		ScenarioWorld world;
		world.CreateScenarioAttacker(RVec3(0.0_r, 0.5_r, 0.0_r));

		TelemetryWriter writer(static_cast<uint8_t>(ReplayScenarioId::PlayerLateral), 1, kDeltaTime);
		PlayerInput attackerInput{ 1.0f, 0.0f, 1.0f };
		PlayerInput neutralInput{ 0.0f, 0.0f, 0.0f };

		for (int frame = 0; frame < 300; ++frame) {
			world.Step(attackerInput, neutralInput);
			writer.AddFrame({ CaptureTelemetryRecord(*world.bodyInterface, world.attacker.bodyId) });
		}

		return FinalizeScenario(ReplayScenarioId::PlayerLateral, writer);
	}

	bool RunPlayerHoverScenario()
	{
		ScenarioWorld world;
		world.CreateScenarioAttacker(RVec3(0.0_r, 2.0_r, 0.0_r));

		TelemetryWriter writer(static_cast<uint8_t>(ReplayScenarioId::PlayerHover), 1, kDeltaTime);
		PlayerInput neutralInput{ 0.0f, 0.0f, 0.0f };

		for (int frame = 0; frame < 300; ++frame) {
			world.Step(neutralInput, neutralInput);
			writer.AddFrame({ CaptureTelemetryRecord(*world.bodyInterface, world.attacker.bodyId) });
		}

		return FinalizeScenario(ReplayScenarioId::PlayerHover, writer);
	}
}

int RunReplayScenarios(const std::string& requestedScenario)
{
	InitJolt();

	bool success = true;
	if (requestedScenario == "all") {
		success &= RunPuckFreeSlideScenario();
		success &= RunPuckWallBounceScenario();
		success &= RunPlayerForwardScenario();
		success &= RunPlayerTurnScenario();
		success &= RunPlayerLateralScenario();
		success &= RunPlayerHoverScenario();
		ShutdownJolt();
		return success ? 0 : 1;
	}

	ReplayScenarioId scenarioId{};
	if (!TryParseScenario(requestedScenario, scenarioId)) {
		std::cerr << "Unknown replay scenario: " << requestedScenario << std::endl;
		std::cerr << "Supported scenarios: puck_free_slide, puck_wall_bounce, player_forward, player_turn, player_lateral, player_hover, all" << std::endl;
		ShutdownJolt();
		return 1;
	}

	switch (scenarioId) {
	case ReplayScenarioId::PuckFreeSlide:
		success = RunPuckFreeSlideScenario();
		break;
	case ReplayScenarioId::PuckWallBounce:
		success = RunPuckWallBounceScenario();
		break;
	case ReplayScenarioId::PlayerForward:
		success = RunPlayerForwardScenario();
		break;
	case ReplayScenarioId::PlayerTurn:
		success = RunPlayerTurnScenario();
		break;
	case ReplayScenarioId::PlayerLateral:
		success = RunPlayerLateralScenario();
		break;
	case ReplayScenarioId::PlayerHover:
		success = RunPlayerHoverScenario();
		break;
	case ReplayScenarioId::PuckStickExit:
	case ReplayScenarioId::StickTracking:
	case ReplayScenarioId::FullPlay:
		std::cerr << "Replay scenario is not implemented yet: " << requestedScenario << std::endl;
		success = false;
		break;
	default:
		success = false;
		break;
	}

	ShutdownJolt();
	return success ? 0 : 1;
}
