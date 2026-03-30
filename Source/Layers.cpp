#include "Layers.h"

JPH_SUPPRESS_WARNINGS


void SetupBroadPhaseMapping(BroadPhaseLayerInterfaceTable& bpli)
{
	// Static
	bpli.MapObjectToBroadPhaseLayer(Layers::ICE, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::BOARDS, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::GOAL_POST, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::GOAL_FRAME, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::GOAL_NET, BroadPhaseLayers::STATIC);
	// Dynamic 
	bpli.MapObjectToBroadPhaseLayer(Layers::PUCK, BroadPhaseLayers::DYNAMIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::PLAYER, BroadPhaseLayers::DYNAMIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::PLAYER_MESH, BroadPhaseLayers::DYNAMIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::STICK, BroadPhaseLayers::DYNAMIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::PLAYER_COLLIDER, BroadPhaseLayers::DYNAMIC);
	// Sensors
	bpli.MapObjectToBroadPhaseLayer(Layers::GOAL_TRIGGER, BroadPhaseLayers::SENSOR);
	bpli.MapObjectToBroadPhaseLayer(Layers::PUCK_TRIGGER, BroadPhaseLayers::SENSOR);
	// Unused layers
	bpli.MapObjectToBroadPhaseLayer(Layers::DEFAULT, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::TRANSPARENT_FX, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::IGNORE_RAYCAST, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::LEVEL, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::WATER, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::UI, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::POST_PROCESSING, BroadPhaseLayers::STATIC);
	bpli.MapObjectToBroadPhaseLayer(Layers::NET_CLOTH_COLLIDER, BroadPhaseLayers::STATIC);
}
void SetupCollisionPairs(ObjectLayerPairFilterTable& filter)
{
	// Stick collisions
	filter.EnableCollision(Layers::STICK, Layers::STICK);
	filter.EnableCollision(Layers::STICK, Layers::PUCK);
	// Puck collisions
	filter.EnableCollision(Layers::PUCK, Layers::PUCK);
	filter.EnableCollision(Layers::PUCK, Layers::PLAYER_MESH);
	filter.EnableCollision(Layers::PUCK, Layers::GOAL_POST);
	filter.EnableCollision(Layers::PUCK, Layers::BOARDS);
	filter.EnableCollision(Layers::PUCK, Layers::ICE);
	filter.EnableCollision(Layers::PUCK, Layers::GOAL_NET);
	filter.EnableCollision(Layers::PUCK, Layers::GOAL_FRAME);
	// Player collisions
	filter.EnableCollision(Layers::PLAYER, Layers::PLAYER);
	filter.EnableCollision(Layers::PLAYER, Layers::GOAL_POST);
	filter.EnableCollision(Layers::PLAYER, Layers::BOARDS);
	filter.EnableCollision(Layers::PLAYER, Layers::ICE);
	filter.EnableCollision(Layers::PLAYER, Layers::GOAL_NET);
	filter.EnableCollision(Layers::PLAYER, Layers::PLAYER_COLLIDER);
	// Goal detection
	filter.EnableCollision(Layers::GOAL_TRIGGER, Layers::PUCK_TRIGGER);
}
