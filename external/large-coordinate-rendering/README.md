# Precision-Safe Rendering of Large-Coordinate CAD Drawings in Three.js

Rendering CAD or GIS data in Three.js becomes challenging when coordinates reach **1e8–1e9 scale**.
This project demonstrates a **two-layer precision strategy**:
1.  **Geometry Rebase (CPU-side)** → prevent precision loss when uploading to GPU
1.  **Relative-to-Eye Rendering (GPU-side)** → prevent precision loss during shader computation

## 🌐 Live Demo
<https://mlightcad.github.io/large-coordinate-rendering/>

## 🎯 What This Demo Shows
The demo provides three rendering modes:
-   **Raw**  
    Large coordinates are directly uploaded to `BufferGeometry` → ❌ broken
-   **Rebased**  
    Geometry is shifted to a local origin before upload → ✅ mostly fixed
-   **Rebased + Relative-to-Eye**  
    Geometry is rebased AND shader is patched to use camera-relative math → ✅ fully robust

## 🧪 What The Demo Draws
All geometry is intentionally placed near:
```
(4_000_000_000, 4_000_000_000, 0)
```
Scene includes:
-   Large outer circle
-   Small inner circle
-   Crosshair lines
-   Stair-step polyline (64-unit increments)
-   Small 64×64 inspection frames

These tiny features are ideal for exposing Float32 precision loss.

# 1. The Root Problem: Float32 Precision Collapse
JavaScript uses **float64**, but GPU uses **float32**.
At around:
```
4_000_000_000
```
Float32 precision step (ULP) is ~hundreds.
So values like:
```
4_000_000_000
4_000_000_064
4_000_000_128
```
collapse to the same number.

## 🔥 Where Precision Is Actually Lost
This line is the real breaking point:
```
const geometry = new THREE.BufferGeometry().setFromPoints(points);
```
Because:
-   Data is copied into `Float32Array`
-   Precision is **irreversibly lost**
-   Later transforms cannot fix it

# 2. Geometry Rebase (First Layer Solution)
## ✅ Core Idea
Before creating `BufferGeometry`, shift coordinates:
```
const localPoint = worldPoint.clone().sub(basePoint);
```

## ✅ What This Fixes
Instead of:
```
(4e9 + 1536, 4e9 - 1024)
```
GPU receives:
```
(1536, -1024)
```
✔️ Small numbers  
✔️ High precision  
✔️ Geometry shape preserved

## ✅ Why This Must Happen Before Geometry Creation
Once data enters `Float32Array`, precision is already gone.
So this **does NOT work**:
```
geometry.applyMatrix4(...)
```
Because:
> ❗ You are transforming already-corrupted data

## ✅ Demo Implementation
```
rootGroup.position.copy(worldBasePoint);
detailGroup.position.copy(detailCenterOffset);
```
-   Geometry stays local
-   Scene graph restores world placement

## ⚠️ Limitation of Rebase
Even after rebasing:
```
rootGroup.position = (4e9, 4e9, 0)
```
This large value still participates in GPU calculations.

# 3. The Remaining Problem
Rebase ensures:
✔️ Vertex buffer is precise
But does NOT guarantee:
❌ GPU math is precise

## ❗ Where Problems Still Appear
-   Batched / instanced rendering
-   Custom shaders reconstructing world position
-   Deep transform hierarchies
-   Line2 / wide line shaders
-   Text / annotation systems

## 🧠 Key Insight
> **Rebase fixes data precision**  
> **But not computation precision**

# 4. Relative-to-Eye (Second Layer Solution)

This demo implements an improved **Relative-to-Eye (RTE)** approach that avoids large floating-point errors in GPU calculations.

## ✅ Core Idea

Instead of computing world position like this:

```
worldPosition = modelMatrix * position
```

We split the transformation into:

```
worldPosition ≈ (modelMatrix without translation) * position
              + (objectWorld - cameraWorld)
```

## 🔥 Key Difference from Basic RTE

A naïve RTE implementation does:

```
relativePosition = (modelMatrix * position) - cameraPosition
```

❌ Problem:

-   `modelMatrix * position` still contains **large values (1e9+)**
-   Precision is already lost before subtraction

## ✅ This Demo Uses: Split Translation RTE

We **remove large translation from GPU entirely**, and only add a **small relative offset**:

```
modelMatrix = [ R | T ]

→ split into:

R (rotation/scale)  → GPU
T (translation)     → CPU (high precision)
```

## ✨ Shader Patch Used in This Demo

### ❌ Default Three.js

```
vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
```

### ✅ Replaced With

```
vec3 localPosition = transformed;

#ifdef USE_BATCHING
  localPosition = ( batchingMatrix * vec4(localPosition, 1.0) ).xyz;
#endif

#ifdef USE_INSTANCING
  localPosition = ( instanceMatrix * vec4(localPosition, 1.0) ).xyz;
#endif

// 1. Remove translation from modelMatrix
mat4 modelNoTranslation = modelMatrix;
modelNoTranslation[3].xyz = vec3(0.0);

// 2. Apply only rotation/scale (safe, small values)
vec3 worldNoTranslation =
  ( modelNoTranslation * vec4(localPosition, 1.0) ).xyz;

// 3. Add relative translation (small, CPU-computed)
vec3 relativePosition =
  worldNoTranslation + uDemoRelativeWorldPosition;

// 4. Apply camera rotation only (no translation)
vec4 mvPosition =
  uDemoViewRotationMatrix * vec4(relativePosition, 1.0);

gl_Position = projectionMatrix * mvPosition;
```

## 🎯 What Each Uniform Means

-   `uDemoRelativeWorldPosition`  
    → `(objectWorldPosition - cameraWorldPosition)`  
    → computed on CPU in **double precision**
-   `uDemoViewRotationMatrix`  
    → view matrix **without translation**  
    → keeps camera orientation but avoids large values

## 🧠 What Changed Conceptually

### ❌ Traditional pipeline

```
local → world (large) → view → projection
```

### ❌ Basic RTE (still flawed)

```
local → world (large) → subtract camera → view → projection
```

👉 still suffers from precision loss

### ✅ This Demo (Split Translation RTE)

```
local → rotate/scale only (small)
     → + (object - camera) (small)
     → view rotation
     → projection
```

✔️ No large numbers on GPU  
✔️ All math stays in Float32-safe range  
✔️ Stable at 1e9+ world scale

## 📌 Why This Works

Example:

```
objectWorld = 4e9 + 5000
cameraWorld = 4e9
```

CPU computes:

```
relative = 5000
```

GPU only sees:

```
~5000 (safe)
```

## 🚀 Result

| Method                                | Large Coordinates | Small Detail Stability |
| ------------------------------------- | ----------------- | ---------------------- |
| Raw                                   | ❌                 | ❌                      |
| Rebase                                | ⚠️                | ⚠️                     |
| Basic RTE                             | ⚠️                | ⚠️                     |
| **Split Translation RTE (this demo)** | ✅                 | ✅

# 5. Relation to CesiumJS
This demo implements a simplified version of the same ideas used in Cesium:
| Technique                 | In This Demo | In Cesium |
| ------------------------- | ------------ | --------- |
| Geometry rebase           | ✅            | ✅         |
| Camera-relative rendering | ✅            | ✅         |
| High/Low precision split  | ❌            | ✅         |
| Matrix splitting          | ❌            | ✅         |

## 📌 Key Difference
This demo stops at:
-   rebase
-   relative-to-eye
Which is sufficient for most CAD use cases (1e4–1e9 range).

# 6. Final Summary
Precision-safe rendering requires **two layers working together**:

## ✅ Layer 1 — Geometry Rebase
-   Prevents precision loss when uploading to GPU
-   Keeps vertex buffers accurate

## ✅ Layer 2 — Relative-to-Eye
-   Prevents precision loss during shader computation
-   Keeps all GPU math in small ranges
