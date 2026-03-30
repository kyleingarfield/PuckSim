#pragma once
#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include "Layers.h"
using namespace JPH;
#include <iostream>
using namespace std;

class MyContactListener : public ContactListener
{
public:
	// Puck body IDs that need net damping applied after physics step.
	// OnContactAdded cannot modify velocities (bodies are locked), so we queue them.
	Array<BodyID> netDampingQueue;
	Array<BodyID> stickExitQueue;
	Array<BodyID> puckStickContactQueue;

	BodyID puckBodyId;
	BodyID attackerStickId;
	BodyID goalieStickId;


	virtual ValidateResult OnContactValidate(const Body& inBody1, const Body& inBody2, RVec3Arg inBaseOffset, const CollideShapeResult& inCollisionResult) override
	{
		return ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	virtual void OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
	{
		ObjectLayer layer1 = inBody1.GetObjectLayer();
		ObjectLayer layer2 = inBody2.GetObjectLayer();

		// Goal scoring detection
		if ((layer1 == Layers::GOAL_TRIGGER && layer2 == Layers::PUCK_TRIGGER) ||
			(layer1 == Layers::PUCK_TRIGGER && layer2 == Layers::GOAL_TRIGGER))
		{
			const Body& goal_body = (layer1 == Layers::GOAL_TRIGGER) ? inBody1 : inBody2;
			float goal_z = goal_body.GetPosition().GetZ();

			if (goal_z > 0)
				cout << "GOAL SCORED - Blue goal (far, +Z)" << endl;
			else
				cout << "GOAL SCORED - Red goal (near, -Z)" << endl;
		}

		// Net damping: queue puck for velocity reduction after physics step
		if ((layer1 == Layers::GOAL_NET && layer2 == Layers::PUCK) ||
			(layer1 == Layers::PUCK && layer2 == Layers::GOAL_NET))
		{
			const Body& puck_body = (layer1 == Layers::PUCK) ? inBody1 : inBody2;
			netDampingQueue.push_back(puck_body.GetID());
		}

		// Puck-stick contact: track for inertia tensor switching
		if ((layer1 == Layers::STICK && layer2 == Layers::PUCK) ||
			(layer1 == Layers::PUCK && layer2 == Layers::STICK))
		{
			puckStickContactQueue.push_back(puckBodyId);
		}
	}

	virtual void OnContactPersisted(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
	{
		ObjectLayer layer1 = inBody1.GetObjectLayer();
		ObjectLayer layer2 = inBody2.GetObjectLayer();

		// Puck-stick contact persisting: track for inertia tensor switching
		if ((layer1 == Layers::STICK && layer2 == Layers::PUCK) ||
			(layer1 == Layers::PUCK && layer2 == Layers::STICK))
		{
			puckStickContactQueue.push_back(puckBodyId);
		}
	}

	virtual void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override
	{
		BodyID id1 = inSubShapePair.GetBody1ID();
		BodyID id2 = inSubShapePair.GetBody2ID();

		bool hasPuck = (id1 == puckBodyId || id2 == puckBodyId);
		bool hasStick = (id1 == attackerStickId || id2 == attackerStickId || id1 == goalieStickId || id2 == goalieStickId);

		if (hasPuck && hasStick) {
			stickExitQueue.push_back(puckBodyId);
		}
	}
};

class MyBodyActivationListener : public BodyActivationListener
{
public:
	virtual void OnBodyActivated(const BodyID& inBodyID, uint64 inBodyUserData) override
	{
	}

	virtual void OnBodyDeactivated(const BodyID& inBodyID, uint64 inBodyUserData) override
	{
	}
};
