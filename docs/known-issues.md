# Known Issues and TODO

## Phase 4: Hover PID

- **RESOLVED: Capsule geometry offset.** The Unity capsule collider has Center: (0, 1.25, 0), meaning the geometry is shifted above the transform position. Fixed by using `RotatedTranslatedShape` instead of `OffsetCenterOfMassShape`. This also corrected the COM position since it naturally follows the geometry.

- **RESOLVED: Raycast distance and target.** Now using the game's exact values: raycast distance 1.45, target distance 1.0, raycast offset (0, 1, 0). Spawn position lowered to Y=0.5 so the ray can reach the ice during initial fall (the game likely spawns players at hover height, not above it).

- **Steady-state hover offset.** The PD controller (no integral term) has a steady-state error of gravity/P_gain = 19.62/100 = 0.1962. This means the hover distance is always ~0.2 less than the target. Body origin settles at Y=-0.17 with capsule bottom at Y=0.33. This is inherent to the PD design and identical in the game.

- **PLAYER vs ICE collision.** Currently enabled as safety net. At hover equilibrium the capsule bottom (Y=0.33) is well above ice (Y=0.025), so the collision doesn't interfere with hovering. It only activates if the player falls below hover height.

- **Movement and drag not yet integrated.** Movement mechanics were tested and verified (acceleration to 7.5 max speed, manual drag at 0.025) but the test code was removed. Will be re-added with proper input handling in a later phase.

- **Angular damping is 0 with no angular drag implementation.** The game handles angular drag manually like linear drag, but we haven't implemented angular drag yet. Not needed until the player is actually rotating from gameplay forces.
