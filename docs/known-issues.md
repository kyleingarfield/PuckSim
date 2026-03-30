# Known Issues and TODO

## Phase 4: Hover PID Calibration

- **Raycast distance is 5.0 instead of game's 1.45.** The game's raycast distance of 1.45 combined with the raycast offset of (0, 1, 0) is too short to reach the ice from the player's starting position or natural hover height in our setup. Temporarily increased to 5.0 to get the PID working. Needs investigation into why the game's value works in Unity but not here — likely related to differences in how the capsule is positioned relative to the ice.

- **Hover target distance may need recalibration.** Currently 1.2 (hoverDistance * balance from game). The PID equilibrium puts the player body origin at ~Y=0.029 without ice collision, which means the capsule clips through the ice. With PLAYER vs ICE collision enabled, the capsule rests on ice at Y=0.755 and the PID never fires (raycast distance > target, so force clamps to 0). The hover will engage during dynamic situations (bouncing, movement) but doesn't hold the player up on its own at the correct height.

- **PLAYER vs ICE collision is acting as the primary floor.** The hover PID should be the main thing keeping the player above ice, with collision as a safety net. Currently it's reversed. This may require adjusting the target distance, raycast offset, or understanding how Unity positions the capsule pivot differently.

- **Movement and drag not yet integrated.** Movement mechanics were tested and verified (acceleration to 7.5 max speed, manual drag at 0.025) but the test code was removed. Will be re-added with proper input handling in a later phase.

- **Angular damping is 0 with no angular drag implementation.** The game handles angular drag manually like linear drag, but we haven't implemented angular drag yet. Not needed until the player is actually rotating from gameplay forces.
