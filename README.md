# Eurekiel: Starship

<p align="center">
<img alt="Realm Select" src="https://github.com/user-attachments/assets/b5703c0a-7e44-47b1-8078-b2cabc58b06e" align="center" width=256>
</p>

<h4 align="center">A space shooter where you control a spaceship to destroy asteroids and survive dangers.</h4>

<p align="center">
<a href="https://www.codefactor.io/repository/github/caishangqi/charming-realm-system"><img src="https://www.codefactor.io/repository/github/caishangqi/charming-realm-system/badge" alt="CodeFactor" /></a>
<img alt="Lines of code" src="https://img.shields.io/tokei/lines/github/Caishangqi/charming-realm-system">
<img alt="Lines of code" src="https://img.shields.io/badge/Engine-Eurekiel-purple">
<img alt="GitHub branch checks state" src="https://img.shields.io/github/checks-status/Caishangqi/charming-realm-system/master?label=build">
<img alt="GitHub code size in bytes" src="https://img.shields.io/github/languages/code-size/Caishangqi/charming-realm-system">
</p>

## Game Overview
Starship Prototype is a classic space shooter where players control a spaceship to survive by destroying asteroids and enemy ships. The game is based on a physics simulation, requiring players to maneuver the ship, turn, thrust, and fire. The goal is to survive in a hazardous space environment by skillfully controlling the ship and eliminating all hostile targets. The game is built on the custom 2D game engine, [Eurekiel](https://github.com/Caishangqi/Eurekiel).
## Gameplay

### Ship Controls

- Press `S` or `F` to rotate the ship.
- Press `E` to thrust the ship forward.
- Press `Spacebar` to fire bullets.
- Press `N` to respawn the ship after destruction.

### Physics Effects
- The ship will bounce when hit, with changes to its direction and speed.
- Asteroids will drift and rotate over time, taking damage upon collisions with bullets or the ship.
- Both the ship and asteroids follow the laws of linear momentum, with no angular momentum involved.

### Win and Loss Conditions
- The player wins by destroying all asteroids.
- The ship can respawn after being destroyed, allowing the player to continue battling asteroids to survive.

## Key Features
- **Dynamic Asteroid Generation:** At the start of the game, six asteroids are generated randomly. Each asteroid has a unique shape and movement pattern. Players can gradually destroy asteroids, with each having 3 health points.
- **Unlimited Shooting:** Players can fire bullets by pressing the spacebar. Bullets fly in the forward direction of the ship until they hit a target or go offscreen.
- **Screen Edge Bouncing:** When the ship touches the edge of the screen, it bounces back, adjusting its speed and direction, adding to the game's challenge.

## Controls
- `S` Key: Rotate the ship left
- `F` Key: Rotate the ship right
- `E` Key: Thrust the ship forward
- `Spacebar`: Fire bullets
- `N` Key: Respawn the ship after destruction
- `I` Key: Spawn a new asteroid
- `F1` Key: Toggle developer mode
- `F8` Key: Restart the game

## Known Issues
- Game performance may degrade when too many asteroids or bullets are present.
- The ship may experience slight physics jitter at high speeds.

## Installation and Execution
- Download and extract the game files.
- Run `Starship_x64.exe` to start the game.
- Use the controls listed above to start playing and defeat as many asteroids as possible.


