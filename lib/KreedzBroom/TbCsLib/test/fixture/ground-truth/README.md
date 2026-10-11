# Ground-truth fixtures

Recorded from Counter-Strike 1.6 itself while TbCsLib was developed. `tst_GroundTruth.cpp`
checks TbCsLib against them.

| File | What it is |
|---|---|
| `tbt_basic.bsp` | A test map with ramps, steps, walls, a surf ramp, a ladder, and water, compiled with VHLT. |
| `tbt_basic_10ms.txt` | A script with 16 movement scenarios at fps_max 100. |
| `tbt_basic_10ms.truth` | The player state after every command of that script, logged by Valve's dedicated server running the Steam client's `cs.so`. |
| `steam_99.5fps.cmds` | The user commands of a demo recorded with the Steam client at fps_max 99.5. |

Scripts are plain text. `reset x y z pitch yaw` puts the player at rest, standing, with
the movement state cleared. It can be followed by
`vx vy vz flags in_duck duck_time fuser2 fall_velocity oldbuttons view_ofs_z` to start
moving. `cmd msec pitch yaw forward side up buttons` is one user command, already
quantized like the network does it (16-bit angles, integer moves). `# name` names the
next scenario.

Each log line is `command origin[3] velocity[3] flags in_duck duck_time fuser2
fall_velocity waterlevel view_ofs_z maxspeed`, with floats written in hexadecimal.
`reset` starts a scenario, and `done` ends the log.

Each `.cmds` line holds a demo frame's `msec`, its buttons, and its forward, side and
up moves. The client input test presses and releases the movement keys that the buttons
show. It then rebuilds each command and expects the result to match the recorded one.
