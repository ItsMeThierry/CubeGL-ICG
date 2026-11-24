#include "mesher.h"

#include "types.h"

#include <array>
#include <cstdint>

// ---------------------------------------------------------------------------
// Tabela de faces.
//
// Cada face tem 4 cantos (0..3). Para cada canto, a posicao (cx,cy,cz) no
// cubo unitario (0 ou 1) e o UV (u,v) na textura.
//
// O UV foi escolhido por face para a textura ficar "em pe" vista de fora.
// A ORDEM dos cantos 0..3 NAO e uniforme entre as faces (algumas estao CW,
// outras CCW vista de fora). Por isso emit_face calcula o winding face a
// face, comparando o produto vetorial dos cantos com a normal da face.
//
// Convencao de UV:
//   topo (+Y):  U=+X, V=+Z
//   base (-Y):  U=+X, V=-Z
//   leste(+X):  U=-Z, V=-Y
//   oeste(-X):  U=+Z, V=-Y
//   sul  (+Z):  U=-X, V=-Y
//   norte(-Z):  U=+X, V=-Y
// ---------------------------------------------------------------------------

namespace {

struct FaceDef {
    int dx, dy, dz;                 // normal
    int cx[4], cy[4], cz[4];        // posicao dos 4 cantos
    int cu[4], cv[4];               // UV dos 4 cantos
};

constexpr FaceDef FACES[6] = {
    // TOPO (+Y): U=+X, V=+Z
    {  0, +1,  0,
       {0,1,1,0}, {1,1,1,1}, {0,0,1,1},
       {0,1,1,0}, {0,0,1,1} },
    // BASE (-Y): U=+X, V=-Z
    {  0, -1,  0,
       {0,1,1,0}, {0,0,0,0}, {1,1,0,0},
       {0,1,1,0}, {0,0,1,1} },
    // LESTE (+X): U=-Z, V=-Y
    { +1,  0,  0,
       {1,1,1,1}, {0,0,1,1}, {1,0,0,1},
       {0,1,1,0}, {0,0,1,1} },
    // OESTE (-X): U=+Z, V=-Y
    { -1,  0,  0,
       {0,0,0,0}, {0,0,1,1}, {0,1,1,0},
       {0,1,1,0}, {0,0,1,1} },
    // SUL (+Z): U=-X, V=-Y
    {  0,  0, +1,
       {1,0,0,1}, {0,0,1,1}, {1,1,1,1},
       {0,1,1,0}, {0,0,1,1} },
    // NORTE (-Z): U=+X, V=-Y
    {  0,  0, -1,
       {0,1,1,0}, {0,0,1,1}, {0,0,0,0},
       {0,1,1,0}, {0,0,1,1} },
};

inline std::uint16_t vertex_light(const Chunk& chunk, int /*lx*/, int /*ly*/,
                                  int /*lz*/) {
    (void)chunk;
    return 0;
}

// Produto vetorial (b - a) x (c - a). Usado para descobrir o winding.
struct IVec3 { int x, y, z; };

inline IVec3 sub(const IVec3& a, const IVec3& b) {
    return IVec3{ a.x - b.x, a.y - b.y, a.z - b.z };
}
inline IVec3 cross(const IVec3& a, const IVec3& b) {
    return IVec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
inline int dot(const IVec3& a, const IVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace

// ---------------------------------------------------------------------------
// neighbor_block
// ---------------------------------------------------------------------------

BlockID Mesher::neighbor_block(const Chunk& self,
                               const Chunk* const neighbors[6],
                               int lx, int ly, int lz,
                               int face) const {
    const FaceDef& f = FACES[face];
    const int S = world_const::CHUNK_SIZE;
    int nx = lx + f.dx;
    int ny = ly + f.dy;
    int nz = lz + f.dz;

    if (nx >= 0 && nx < S && ny >= 0 && ny < S && nz >= 0 && nz < S) {
        return (BlockID)self.get(nx, ny, nz);
    }

    const Chunk* nb = neighbors[face];
    if (!nb) return block_id::AR;

    if (nx < 0) nx = S - 1; else if (nx >= S) nx = 0;
    if (ny < 0) ny = S - 1; else if (ny >= S) ny = 0;
    if (nz < 0) nz = S - 1; else if (nz >= S) nz = 0;
    return (BlockID)nb->get(nx, ny, nz);
}

// ---------------------------------------------------------------------------
// emit_face
// ---------------------------------------------------------------------------

void Mesher::emit_face(const Chunk& chunk, int lx, int ly, int lz,
                       int face, int layer,
                       bool transparent,
                       MeshData& out) const {
    std::vector<Vertex>&     verts = transparent ? out.transparent_vertices
                                                 : out.opaque_vertices;
    std::vector<std::uint32_t>& idx = transparent ? out.transparent_indices
                                                  : out.opaque_indices;

    const FaceDef& f = FACES[face];
    const std::uint32_t base = (std::uint32_t)verts.size();

    for (int k = 0; k < 4; ++k) {
        Vertex v;
        v.pos_x = (std::uint8_t)(lx + f.cx[k]);
        v.pos_y = (std::uint8_t)(ly + f.cy[k]);
        v.pos_z = (std::uint8_t)(lz + f.cz[k]);
        v.face_uv = pack_face_uv(face, f.cu[k], f.cv[k]);
        v.layer = (std::uint8_t)layer;
        v.light = vertex_light(chunk, lx, ly, lz);
        verts.push_back(v);
    }

    // Descobre o winding da ordem 0,1,2,3 comparando o produto vetorial dos
    // tres primeiros cantos com a normal da face. Se o produto apontar na
    // direcao da normal, a ordem 0..3 ja e CCW vista de fora; senao e CW.
    //
    // A normal e (f.dx, f.dy, f.dz). O produto vetorial e calculado em
    // coordenadas locais do cubo (os cantos cx/cy/cz).
    const IVec3 p0{ f.cx[0], f.cy[0], f.cz[0] };
    const IVec3 p1{ f.cx[1], f.cy[1], f.cz[1] };
    const IVec3 p2{ f.cx[2], f.cy[2], f.cz[2] };
    const IVec3 normal{ f.dx, f.dy, f.dz };
    const IVec3 n = cross(sub(p1, p0), sub(p2, p0));
    const bool ccw = dot(n, normal) > 0;

    if (ccw) {
        // 0..3 ja e CCW: emite 0,1,2 / 0,2,3.
        idx.push_back(base + 0);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
        idx.push_back(base + 0);
        idx.push_back(base + 2);
        idx.push_back(base + 3);
    } else {
        // 0..3 e CW: inverte para 0,2,1 / 0,3,2.
        idx.push_back(base + 0);
        idx.push_back(base + 2);
        idx.push_back(base + 1);
        idx.push_back(base + 0);
        idx.push_back(base + 3);
        idx.push_back(base + 2);
    }
}

// ---------------------------------------------------------------------------
// build_chunk_mesh
// ---------------------------------------------------------------------------

MeshData Mesher::build_chunk_mesh(const World& world, const ChunkPos& cp) const {
    MeshData out;
    const Chunk* chunk = world.get_chunk(cp);
    if (!chunk) return out;

    const int S = world_const::CHUNK_SIZE;

    // Cache dos 6 vizinhos de face na ordem de face_index:
    // 0=TOPO(+Y), 1=BASE(-Y), 2=LESTE(+X), 3=OESTE(-X), 4=SUL(+Z), 5=NORTE(-Z).
    const Chunk* neighbors[6] = {
        (cp.y + 1 < world_const::CHUNKS_Y) ? world.get_chunk({cp.x, (std::int16_t)(cp.y + 1), cp.z}) : nullptr,
        (cp.y - 1 >= 0)                    ? world.get_chunk({cp.x, (std::int16_t)(cp.y - 1), cp.z}) : nullptr,
        world.get_chunk({(std::int16_t)(cp.x + 1), cp.y, cp.z}),
        world.get_chunk({(std::int16_t)(cp.x - 1), cp.y, cp.z}),
        world.get_chunk({cp.x, cp.y, (std::int16_t)(cp.z + 1)}),
        world.get_chunk({cp.x, cp.y, (std::int16_t)(cp.z - 1)}),
    };

    for (int ly = 0; ly < S; ++ly) {
        for (int lz = 0; lz < S; ++lz) {
            for (int lx = 0; lx < S; ++lx) {
                const BlockID id = (BlockID)chunk->get(lx, ly, lz);
                if (id == block_id::AR) continue;

                const BlockInfo& info = block_info(id);
                const bool is_transparent = info.transparent;

                for (int face = 0; face < 6; ++face) {
                    const BlockID nb = neighbor_block(*chunk, neighbors, lx, ly, lz, face);
                    const BlockInfo& ninfo = block_info(nb);

                    bool emit;
                    if (!is_transparent) {
                        emit = ninfo.transparent;
                    } else {
                        if (nb == block_id::AR) {
                            emit = true;
                        } else if (ninfo.transparent) {
                            emit = (nb != id);
                        } else {
                            emit = false;
                        }
                    }
                    if (!emit) continue;

                    int layer = 0;
                    switch (face) {
                        case face_index::TOPO:  layer = info.tex_top;    break;
                        case face_index::BASE:  layer = info.tex_bottom; break;
                        case face_index::LESTE: layer = info.tex_east;   break;
                        case face_index::OESTE: layer = info.tex_west;   break;
                        case face_index::SUL:   layer = info.tex_south;  break;
                        case face_index::NORTE: layer = info.tex_north;  break;
                    }

                    emit_face(*chunk, lx, ly, lz, face, layer,
                              is_transparent, out);
                }
            }
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// compute_affected_chunks
// ---------------------------------------------------------------------------

void Mesher::compute_affected_chunks(const World& /*world*/,
                                     int wx, int wy, int wz,
                                     std::vector<ChunkPos>& out) const {
    out.clear();
    if (wy < 0 || wy >= world_const::WORLD_HEIGHT) return;

    const ChunkPos cp = world_to_chunk(wx, wy, wz);
    out.push_back(cp);

    const int wx_n[6] = { wx, wx, wx - 1, wx + 1, wx, wx };
    const int wy_n[6] = { wy - 1, wy + 1, wy, wy, wy, wy };
    const int wz_n[6] = { wz, wz, wz, wz, wz - 1, wz + 1 };
    for (int i = 0; i < 6; ++i) {
        if (wy_n[i] < 0 || wy_n[i] >= world_const::WORLD_HEIGHT) continue;
        ChunkPos n = world_to_chunk(wx_n[i], wy_n[i], wz_n[i]);
        bool dup = false;
        for (const auto& e : out) if (e == n) { dup = true; break; }
        if (!dup) out.push_back(n);
    }
}