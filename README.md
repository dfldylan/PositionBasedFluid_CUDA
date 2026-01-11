# PositionBasedFluid_CUDA
Position Based Fluid Simulation Using CUDA.


Blog(In Chinese):https://yangwc.com/2019/06/26/PBF_CUDA/


![demo.gif](/picture/demo.gif)

---

## Headless Service Mode

This project supports running as a headless service: starts a TCP server listening on port 7777, communicates via line-delimited JSON protocol, no GUI required.

**Key Features:**
- Listen address: `0.0.0.0:7777`
- Protocol: One JSON message per line (UTF-8), terminated by `\n`
- Architecture:
  - `SocketServer`: Manages TCP connections and line-delimited JSON I/O
  - `FluidService`: Maintains simulation state, handles commands (reset, add particles, step, export state, etc.)
  - `main.cpp`: Service entry point, runs in blocking loop without GUI

Startup log:
```
Fluid service started at 0.0.0.0:7777
Protocol: one JSON per line. Field 'type' selects operation.
```

### Build & Run (Windows)

**Requirements:**
- Visual Studio 2019/2022 (with C++ toolset)
- CUDA Toolkit (matching project configuration)
- Dependencies in `lib/` (glew, glfw, assimp, etc.)

**Steps:**
1. Open `PBF_CUDA.sln` in Visual Studio
2. Select `x64` configuration (recommend `Release`)
3. Set `PBF_CUDA` as startup project
4. Build and run
5. Process enters listening state (no window)

**Note:** Executable can be run directly from build directory (e.g., `x64/Release/`).

### Protocol Overview

- Field `type` (integer) is mandatory to identify message type
- All requests and responses are single-line JSON
- Vectors: `[x, y, z]`; Quaternions: `[w, x, y, z]`
- Failure response: `{"ok": false, "msg": "..."}`
- **Message Types:**
  - `1` - Reset scene
  - `2` - Add fluid particles (point cloud)
  - `4` - Step simulation and return state
  - `5` - Clear all fluids and solids
  - `6` - Get current state (no simulation step)
  - `7` - Upsert solid point cloud

---

#### Type 1: Reset Scene

Clears current scene and reinitializes with given configuration. Optionally updates particle radius and capacity.

**Request:**
```json
{"type":1, "gridSize":64, "radius":0.02, "numParticles":200000}
```

**Response:**
```json
{"ok":true, "config":{"gridSize":64, "radius":0.02, "numParticles":200000}}
```

**Note:** Omitted fields retain current values.

---

#### Type 2: Add Fluid Particles

Adds fluid particles as point cloud. Supports batch addition; velocities optional (default `[0,0,0]`).

**Request (with velocities):**
```json
{
  "type": 2,
  "fluid": {
    "positions": [[0,1,0], [0.02,1,0], [0.04,1,0]],
    "velocities": [[0,0,0], [0,0,0], [0,0,0]]
  }
}
```

**Request (without velocities):**
```json
{
  "type": 2,
  "fluid": {
    "positions": [[0,1,0], [0.02,1,0], [0.04,1,0]]
  }
}
```

**Response:**
```json
{"ok":true, "added":3}
```

---

#### Type 4: Step Simulation

Advances simulation by `dt` seconds. Optionally updates rigid body transforms for specified solids. Returns updated positions and velocities.

**Request:**
```json
{
  "type": 4,
  "dt": 0.016,
  "solids": [
    {"id": "paddle", "rotation": [1,0,0,0], "translation": [0,0,0]}
  ]
}
```

**Response:**
```json
{
  "ok": true,
  "positions": [x1,y1,z1, x2,y2,z2, ...],
  "velocities": [vx1,vy1,vz1, vx2,vy2,vz2, ...]
}
```

**Note:** Arrays are flattened XYZ, length = `3 * particle_count`.

---

#### Type 5: Clear

Clears all fluid particles and registered solids.

**Request:**
```json
{"type":5}
```

**Response:**
```json
{"ok":true, "msg":"cleared"}
```

---

#### Type 6: Get State

Returns current particle positions and velocities without advancing simulation.

**Request:**
```json
{"type":6}
```

**Response:**
```json
{
  "ok": true,
  "positions": [...],
  "velocities": [...]
}
```

---

#### Type 7: Upsert Solid Point Cloud

Registers or updates a solid object as point cloud. Each solid requires matching `positions[N,3]` and `normals[N,3]` arrays.

**Request:**
```json
{
  "type": 7,
  "solid": {
    "id": "ground_pc",
    "positions": [[0,0,0],[0.05,0,0],[0.1,0,0]],
    "normals": [[0,1,0],[0,1,0],[0,1,0]]
  }
}
```

**Response:**
```json
{"ok":true, "id":"ground_pc", "count":3}
```

---

### Best Practices

- Messages must end with `\n`; do not send multiline JSON
- Use consistent unit system; recommended `dt`: 1/60 ~ 1/120 seconds
- Default velocity is `[0,0,0]` if not specified
- Solids stored as point clouds; collision/constraint handling can be extended
- Single-process, single-port service; no authentication—use in trusted network only

### Example Workflow

```text
{"type":1, "gridSize":64, "radius":0.02, "numParticles":200000}
{"type":2, "fluid": {"positions": [[0,1,0],[0.02,1,0],[0.04,1,0]]}}
{"type":7, "solid": {"id":"ground", "positions": [[0,0,0],[0.05,0,0]], "normals": [[0,1,0],[0,1,0]]}}
{"type":4, "dt":0.016}
{"type":6}
{"type":5}
```

Connect to `127.0.0.1:7777` using any line-delimited JSON TCP client.

### Error Handling

- Failure response: `{"ok":false, "msg":"..."}`
- Common causes:
  - JSON syntax error or missing required fields (`type`, `dt`, etc.)
  - Invalid parameters (mismatched array lengths, unregistered `id`)

