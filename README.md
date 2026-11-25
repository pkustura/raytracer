# raytracer

A small real-time CPU raytracer in C++ that draws to a GLFW window.

## Building

Requires CMake, GLFW and GLEW (e.g. `brew install cmake glfw glew`).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/Raytracer
```

A Release build is strongly recommended. The renderer runs on the CPU every frame.

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
Mesh dragon = Mesh::from_obj("objs/dragon.obj", phongMat, Vector3(0, 0, -1), 0.5f);
shapes.push_back(new Mesh(dragon));
```

Paths are relative to the directory you run the program from.

### Included models (`objs/`)

| File | Triangles |
| --- | --- |
| `dragon.obj` | 238,954 |
| `buddha.obj` | 112,876 |
| `teapot_chinese.obj` | 2,182 |
| `torus.obj` | 72 |

The **dragon** and **Buddha** models come from the [Stanford 3D Scanning Repository](https://graphics.stanford.edu/data/3Dscanrep/), Stanford Computer Graphics Laboratory. The files here are reduced-resolution versions of the original scans.
