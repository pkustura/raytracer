# raytracer

A small real-time, multithreaded CPU raytracer in C++ that draws to a GLFW window.

## Building

Requires CMake, GLFW and GLEW (e.g. `brew install cmake glfw glew`).

```sh
cmake -S . -B build
cmake --build build
./build/Raytracer
```

The build defaults to Release, since the renderer runs on the CPU every frame. Model paths are compiled in as absolute paths to this repo's `objs/` directory, so `Raytracer` and `Benchmark` can be run from any directory. If you move the repo, re-run CMake.

## Scene

The default scene is built in `build_default_scene` (`src/scene.cpp`): the Buddha, the dragon, a mirrored teapot and a glass sphere on a diffuse floor. The interactive app and the benchmark both use it. `Scene` also holds the light and the starting camera position.

### Controls

- **WASD**: move
- **Mouse**: look around
- **Esc**: quit

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

### Included models (`objs/`)

| File | Triangles |
| --- | --- |
| `dragon.obj` | 238,954 |
| `buddha.obj` | 112,876 |
| `teapot_chinese.obj` | 2,182 |
| `torus.obj` | 72 |

The **dragon** and **Buddha** models come from the [Stanford 3D Scanning Repository](https://graphics.stanford.edu/data/3Dscanrep/), Stanford Computer Graphics Laboratory. The files here are reduced-resolution versions of the original scans.

## Benchmark

`build/Benchmark` renders the default scene headlessly. The scene is the dragon, the Buddha, a mirrored teapot and a glass sphere: about 352k triangles in total. The camera does one full orbit over the run, and the program reports average FPS and ray throughput. "All rays" counts secondary rays too (mirror reflections, glass reflection and refraction).

```sh
./build/Benchmark                 # 60 frames at 800x600
./build/Benchmark --frames 120 --width 1280 --height 720 --out frame.ppm
```

### Results log

Machine: Apple M3 Pro (12 cores: 6 performance + 6 efficiency), Release build, 60 frames at 800x600. Each row is the median of 3 runs.

| Change | FPS | ms/frame | Primary Mrays/s | All Mrays/s | Load + BVH build |
| --- | --- | --- | --- | --- | --- |
| Baseline: median-split BVH, 1 thread | 6.75 | 148.1 | 3.24 | 4.47 | 0.28 s |
| Binned SAH BVH (12 bins), 1 thread | 7.64 | 130.9 | 3.67 | 5.05 | 0.35 s |
| SAH BVH + multithreading, 12 threads | 52.05 | 19.2 | 24.98 | 34.43 | 0.35 s |

Notes:

- **SAH (+13%)**: the BVH is built using the surface area heuristic. Triangle centroids are sorted into 12 bins per axis, and the split with the lowest estimated cost, 1 + (A_L·N_L + A_R·N_R) / A_parent, is chosen. A node becomes a leaf when splitting would cost more than testing all of its triangles. Building the tree takes about 70 ms longer. The gain is modest, probably for two reasons: traversal doesn't visit the nearer child first, and `Vector3` operations are function calls in another file that the compiler can't inline. Both are likely next steps.
- **Multithreading (6.8× over SAH on 1 thread)**: `render_frame` starts one worker per hardware thread. Workers take batches of 4 rows from a shared atomic counter, so threads that hit expensive glass and mirror pixels don't hold up the others. The output is byte-identical to the single-threaded render. The three 12-thread runs ranged from 46.0 to 54.8 fps. For scaling: 1 thread gave 7.87 fps, and 6 threads 35.87 fps. Use `--threads N` to compare.
