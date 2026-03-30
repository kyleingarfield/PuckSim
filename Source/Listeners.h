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
	// See: ContactListener
	virtual ValidateResult	OnContactValidate(const Body& inBody1, const Body& inBody2, RVec3Arg inBaseOffset, const CollideShapeResult& inCollisionResult) override
	{

		// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
		return ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	virtual void OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
	{
		ObjectLayer layer1 = inBody1.GetObjectLayer();
		ObjectLayer layer2 = inBody2.GetObjectLayer();

		if ((layer1 == Layers::GOAL_TRIGGER && layer2 == Layers::PUCK_TRIGGER) ||
			(layer1 == Layers::PUCK_TRIGGER && layer2 == Layers::GOAL_TRIGGER))
		{
			// Determine which goal by checking Z position of the goal trigger body
			const Body& goal_body = (layer1 == Layers::GOAL_TRIGGER) ? inBody1 : inBody2;
			float goal_z = goal_body.GetPosition().GetZ();

			if (goal_z > 0)
				cout << "GOAL SCORED — Blue goal (far, +Z)" << endl;
			else
				cout << "GOAL SCORED — Red goal (near, -Z)" << endl;
		}
	}

	virtual void OnContactPersisted(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
	{
	}

	virtual void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override
	{
	}
};

// An example activation listener
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