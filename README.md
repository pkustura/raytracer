# raytracer

A small real-time, multithreaded CPU raytracer and progressive path tracer in C++ that draws to a GLFW window.

## Building

Requires CMake, GLFW and GLEW (e.g. `brew install cmake glfw glew`), plus `curl`, `tar` and `awk` to download the models. [Dear ImGui](https://github.com/ocornut/imgui), used for the settings panel, is downloaded automatically by CMake the first time you configure.

```sh
cmake -S . -B build
cmake --build build      # also downloads any missing models into objs/
./build/Raytracer
```

The build runs `fetch_models.sh`, which downloads only the models that are missing; once they're all present it just prints one line. To build offline, configure with `-DRAYTRACER_FETCH_MODELS=OFF` and run `./fetch_models.sh` yourself when you're back online.

The build defaults to Release, since the renderer runs on the CPU every frame. Model paths are compiled in as absolute paths to this repo's `objs/` directory, so `Raytracer` and `Benchmark` can be run from any directory. If you move the repo, re-run CMake.

## Scene

The default scene is built in `build_default_scene` (`src/scene.cpp`): the Buddha, the dragon, a mirrored Utah teapot and a glass sphere on a diffuse floor. The interactive app and the benchmark both use it. `Scene` also holds the light and the starting camera position.

### Controls

- **WASD**: move (4 units per second, independent of frame rate)
- **Shift**: move 3× faster
- **Mouse**: look around
- **Tab**: open or close the settings panel. While it's open the cursor is free and mouse look is paused.
- **Esc**: quit

### Settings panel

- **Stats**: FPS, render time, ray throughput, render resolution, triangle count and scene build time.
- **Graphics**:
  - **Renderer**: `CPU (Whitted)` or `CPU path tracer` (see below)
  - **Threads**: automatic, or a fixed count
  - **Max bounces**: reflection and refraction depth
  - **Resolution**: render scale, 25–100% of the window
  - **BVH**: SAH or median split (rebuilds the scene)
  - **Reset camera**
- **Path tracing** (path tracer only): sample count, samples per frame, a sample cap (0 = no limit), a caustics toggle, the firefly clamp, and a button to restart accumulation.
- **Benchmark**: runs the same camera orbit as `build/Benchmark` with the current settings and shows the results in a table.

Rendering backends implement the `Renderer` interface (`src/renderer.h`). The CPU renderers are always available, so they're the fallback for future GPU backends.

## Renderers

### CPU (Whitted)

Classic Whitted-style ray tracing (`src/renderer.cpp`): Phong shading from the point light, plus recursive mirror reflection and glass reflection/refraction. Fast, but with hard edges and no shadows.

### CPU path tracer

A progressive path tracer (`src/pathtracer.cpp`). While the camera is still, each frame adds samples to a running average, so the image keeps getting cleaner. Moving the camera, or changing the resolution, bounce count or scene, restarts it. Jittering each sample within its pixel also gives anti-aliasing.

- **Lighting**:
  - **Sky:** the background gradient lights the scene from every direction. This replaces the materials' ambient term.
  - **Sun:** the scene's light becomes a sun disc 3° across, in the same direction. It's sampled directly with a shadow ray at every diffuse or glossy hit, which gives soft shadows with little noise. Its brightness is calibrated so directly lit diffuse surfaces match the Whitted renderer.
- **Materials**:
  - **Diffuse and Phong:** matte (Lambertian) diffuse plus a glossy lobe based on normalized Phong, using the material's shininess. If diffuse plus specular would reflect more than 100% of the light, the specular part is scaled down.
  - **Mirror:** perfect reflection.
  - **Glass:** each sample picks reflection or refraction randomly, weighted by Fresnel reflectance. `transparency` tints the light that passes through.
- **Global illumination:** light bounces between surfaces up to **Max bounces**. After 3 bounces, Russian roulette randomly ends dim paths, without biasing the result.
- **Caustics:** light focused through the glass sphere shows up as a bright spot in its shadow, and the mirrored teapot throws reflected sunlight onto the floor and the dragon. These paths only reach the tiny, very bright sun disc by chance, so they show up as bright speckles ("fireflies") that take many samples to smooth out.
  - **Firefly clamp** (default 20): caps the brightness of any single sample of bounced light, so the speckles go away. Light that hits a surface directly is never clamped. The caustic stays but gets dimmer. Measured at 256 samples: clamp 20 removes all the speckles and keeps about 26% of the caustic's brightness, clamp 50 keeps 52% but some speckles come back, and 0 turns the clamp off (exact but noisy).
  - **Caustics** (on by default): turning it off removes caustic paths entirely, including the caustic under the sphere.

## Materials

- `DiffuseMaterial`: ambient + Lambertian diffuse
- `PhongMaterial`: diffuse + Phong specular highlight
- `MirrorMaterial`: local shading plus a reflection tinted by `mirror_reflectance`
- `GlassMaterial`: reflection and refraction blended with Schlick's Fresnel approximation, including total internal reflection

## Loading .obj models

`load_obj` (`src/obj_loader.h`) reads vertex (`v`) and face (`f`) lines from a Wavefront OBJ file. It ignores texture coordinates, normals and materials, and splits polygons into triangles. By default it centers the model and scales it to fit in [-1, 1].

`Mesh` (`src/mesh.h`) turns that into a shape the raytracer can render. It builds a BVH (bounding volume hierarchy), so models with hundreds of thousands of triangles still render quickly:

```cpp
scene.shapes.push_back(new Mesh(Mesh::from_obj(asset_path("dragon.obj"), phongMat, Vector3(0, 0, -1), 0.5f)));
```

`asset_path("name.obj")` resolves to the repo's `objs/` directory. Inside `scene.cpp`, `add_model(scene, file, material, x, z, floor_y, scale)` also places the model so its lowest point rests on the floor.

### Models (`objs/`)

The models aren't stored in git. The build downloads them from their sources with `fetch_models.sh`. You can also run it directly; `./fetch_models.sh --force` downloads everything again.

| File | Triangles | Source |
| --- | --- | --- |
| `dragon.obj` | 202,520 | Stanford `dragon_recon/dragon_vrip_res2.ply`, converted to OBJ |
| `buddha.obj` | 293,232 | Stanford `happy_recon/happy_vrip_res2.ply`, converted to OBJ |
| `teapot.obj` | 6,320 | Utah teapot, via [common-3d-test-models](https://github.com/alecjacobson/common-3d-test-models) |
| `torus.obj` | 72 | [3dviewer](https://github.com/matheusgadelha/3dviewer) |

Credits:

- **Dragon and Buddha:** [Stanford 3D Scanning Repository](https://graphics.stanford.edu/data/3Dscanrep/), Stanford Computer Graphics Laboratory. Stanford distributes them as PLY files, and `res2` is their second-level decimation of the original scans. The full scans have 871,414 and 1,087,716 triangles.
- **Teapot:** the Utah teapot, modeled by Martin Newell at the University of Utah in 1975. The triangulated OBJ comes from Alec Jacobson's [common-3d-test-models](https://github.com/alecjacobson/common-3d-test-models).
- **Torus:** from Matheus Gadelha's [3dviewer](https://github.com/matheusgadelha/3dviewer), MIT License, © Matheus Gadelha.

`fetch_models.sh` pins the GitHub downloads to specific commits, so the files can't change under you.

## Benchmark

`build/Benchmark` renders the default scene without opening a window. The scene is the dragon, the Buddha, a mirrored teapot and a glass sphere: about 502k triangles in total. The camera does one full orbit over the run, and the program reports average FPS and ray throughput. "All rays" counts secondary rays too (mirror reflections, glass reflection and refraction).

```sh
./build/Benchmark                                  # 60 frames at 800x600, SAH BVH, all threads
./build/Benchmark --bvh median --threads 1         # the original baseline configuration
./build/Benchmark --renderer path --spp 1          # path tracer, 1 sample per pixel per frame
./build/Benchmark --frames 120 --width 1280 --height 720 --out frame.ppm
```

The same benchmark is available inside the app, from the settings panel. Both use `BenchmarkRun` (`src/benchmark.h`).

| Flag | Default | Meaning |
| --- | --- | --- |
| `--frames N` | 60 | frames in one full camera orbit |
| `--width W`, `--height H` | 800, 600 | resolution |
| `--threads T` | 0 | worker threads (0 = one per hardware thread) |
| `--depth D` | 5 | max reflection/refraction bounces |
| `--bvh sah\|median` | `sah` | BVH build strategy |
| `--renderer whitted\|path` | `whitted` | which renderer to benchmark |
| `--spp S` | 1 | samples per pixel per frame (path tracer) |
| `--out file.ppm` | none | save the first frame |

### Results

Machine: Apple M3 Pro (12 cores: 6 performance + 6 efficiency), Release build, 60 frames at 800x600, 1.35 rays per pixel on average. One run per row; runs typically vary by a few percent.

| Configuration | Flags | FPS | ms/frame | Primary Mrays/s | All Mrays/s | Load + BVH build |
| --- | --- | --- | --- | --- | --- | --- |
| Median-split BVH, 1 thread | `--bvh median --threads 1` | 5.90 | 169.5 | 2.83 | 3.82 | 0.41 s |
| SAH BVH, 1 thread | `--bvh sah --threads 1` | 6.67 | 149.8 | 3.20 | 4.32 | 0.51 s |
| SAH BVH, 12 threads | *(defaults)* | 48.19 | 20.8 | 23.13 | 31.19 | 0.53 s |

Overall, 8.2× faster than the baseline.

Path tracer (`--renderer path`; same scene and settings). The camera moves every frame, so each frame is a fresh 1-sample image:

| Configuration | Flags | FPS | ms/frame | Camera Mrays/s | All Mrays/s | Rays per pixel |
| --- | --- | --- | --- | --- | --- | --- |
| Path tracer, 1 spp/frame, 12 threads | `--renderer path` | 14.06 | 71.1 | 6.75 | 22.59 | 3.35 |

Each path-traced sample costs about 3.35 rays: the camera ray, its bounces, and a shadow ray toward the sun at each diffuse or glossy hit.

- **SAH (+13%)**: the BVH is built using the surface area heuristic. Triangle centroids are sorted into 12 bins per axis, and the split with the lowest estimated cost, 1 + (A_L·N_L + A_R·N_R) / A_parent, is chosen. A node becomes a leaf when splitting would cost more than testing all of its triangles. Building the tree takes about 100 ms longer. The gain is modest, probably for two reasons: traversal doesn't visit the nearer child first, and `Vector3` operations are function calls in another file that the compiler can't inline. Both are likely next steps.
- **Multithreading (7.2× over SAH on 1 thread)**: `render_frame` starts one worker per hardware thread. Workers take batches of 4 rows from a shared atomic counter, so threads that hit expensive glass and mirror pixels don't hold up the others. The output is byte-identical to the single-threaded render. With 6 performance and 6 efficiency cores, a speedup below 12× is expected.
