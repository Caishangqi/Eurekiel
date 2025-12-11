#include "RenderMesh.hpp"
#include "../../Math/Mat44.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
using namespace enigma::renderer::model;

void RenderMesh::AddFace(const RenderFace& face)
{
    faces.push_back(face);
    gpuDataValid = false;
}

size_t RenderMesh::GetVertexCount() const
{
    size_t count = 0;
    for (const auto& face : faces)
    {
        count += face.vertices.size();
    }
    return count;
}

size_t RenderMesh::GetIndexCount() const
{
    size_t count = 0;
    for (const auto& face : faces)
    {
        count += face.indices.size();
    }
    return count;
}

const RenderFace* RenderMesh::GetFace(Direction direction) const
{
    for (const auto& face : faces)
    {
        if (face.cullDirection == direction)
        {
            return &face;
        }
    }
    return nullptr;
}

std::vector<const RenderFace*> RenderMesh::GetFaces(Direction direction) const
{
    std::vector<const RenderFace*> result;
    for (const auto& face : faces)
    {
        if (face.cullDirection == direction)
        {
            result.push_back(&face);
        }
    }
    return result;
}

void RenderMesh::CompileToGPU() const
{
}

std::shared_ptr<VertexBuffer> RenderMesh::GetVertexBuffer() const
{
    if (!gpuDataValid)
    {
        CompileToGPU();
    }
    return vertexBuffer;
}

std::shared_ptr<IndexBuffer> RenderMesh::GetIndexBuffer() const
{
    if (!gpuDataValid)
    {
        CompileToGPU();
    }
    return indexBuffer;
}

void RenderMesh::InvalidateGPUData()
{
    gpuDataValid = false;
}

void RenderMesh::TransformAndAppendTo(voxel::ChunkMesh* chunk_mesh, const Vec3& vec3)
{
    UNUSED(chunk_mesh)
    UNUSED(vec3)
}

void RenderMesh::Clear()
{
    faces.clear();
    gpuDataValid = false;
}

void RenderMesh::ApplyBlockRotation(int rotX, int rotY)
{
    // Skip if no rotation needed
    if (rotX == 0 && rotY == 0)
    {
        return;
    }

    // Block center for rotation pivot (model space is 0-1)
    constexpr float CENTER = 0.5f;
    const Vec3      pivot(CENTER, CENTER, CENTER);

    // Build rotation matrix
    // ============================================================================
    // Coordinate System Mapping
    // ============================================================================
    // SimpleMiner: +X=Forward, +Y=Left,  +Z=Up    (Right-handed, Z-up)
    // Minecraft:   +X=East,    +Y=Up,    +Z=South (Right-handed, Y-up)
    //
    // ============================================================================
    // Rotation Axis Mapping
    // ============================================================================
    // - Minecraft Y-axis rotation (vertical) -> SimpleMiner Z-axis rotation
    // - Minecraft X-axis rotation (horizontal) -> SimpleMiner X-axis rotation
    //
    // ============================================================================
    // CRITICAL: Y Rotation Direction Mapping
    // ============================================================================
    // The horizontal planes are different:
    //   MC horizontal: XZ plane (X=EAST, Z=SOUTH)
    //   SM horizontal: XY plane (X=FORWARD, Y=LEFT)
    //
    // Base model orientations differ:
    //   MC base model faces SOUTH(0,0,+1) when y:0
    //   SM base model faces NORTH(0,+1,0) when z:0
    //   (because MC SOUTH maps to SM NORTH in coordinate conversion)
    //
    // Rotation direction analysis:
    //   MC SOUTH(0,0,+1) counter-clockwise 90° -> MC EAST(+1,0,0)
    //   SM NORTH(0,+1,0) must also rotate to -> SM EAST(+1,0,0)
    //
    // SimpleMiner rotation from NORTH to EAST:
    //   NORTH(0,+1,0) counter-clockwise 90° around +Z -> WEST(-1,0,0) [WRONG]
    //   NORTH(0,+1,0) clockwise 90° around +Z -> EAST(+1,0,0) [CORRECT]
    //
    // However, AppendZRotation(+270) = counter-clockwise 270° = clockwise 90°
    // So we can use: AppendZRotation(+rotY) to achieve the correct rotation
    //
    // Conclusion: MC y:angle -> SM z:+angle (DO NOT NEGATE!)
    //
    // ============================================================================
    // X Rotation Mapping
    // ============================================================================
    // X rotation (flip upside-down) requires negation due to axis direction
    // differences between the two coordinate systems.
    // Conclusion: MC x:angle -> SM x:-angle (NEGATE)

    Mat44 rotationMatrix = Mat44::IDENTITY;

    // Apply Y rotation (around Minecraft's Y-axis = SimpleMiner's Z-axis)
    //
    // [CRITICAL] Rotation direction difference:
    // - Minecraft: +Y axis points UP, rotation from +Y looking down = CLOCKWISE
    //   y:90 rotates EAST → SOUTH (clockwise)
    // - SimpleMiner: +Z axis points UP, rotation from +Z looking down = COUNTER-CLOCKWISE
    //   z:+90 rotates EAST → NORTH (counter-clockwise)
    //
    // Therefore: MC y:angle = SM z:-angle (NEGATE!)
    if (rotY != 0)
    {
        core::LogInfo("RenderMesh", "[ApplyBlockRotation] Applying Z rotation: %d degrees (negated from MC y:%d)", -rotY, rotY);
        rotationMatrix.AppendZRotation(static_cast<float>(-rotY));
    }

    // Apply X rotation (around Minecraft's X-axis = SimpleMiner's X-axis)
    // Use -rotX (negate) due to axis direction differences between coordinate systems
    if (rotX != 0)
    {
        rotationMatrix.AppendXRotation(static_cast<float>(-rotX));
    }

    // Transform all vertices in all faces
    for (auto& face : faces)
    {
        for (auto& vertex : face.vertices)
        {
            // Translate to origin (pivot at block center)
            Vec3 pos = vertex.m_position - pivot;

            // Apply rotation
            pos = rotationMatrix.TransformPosition3D(pos);

            // Translate back
            vertex.m_position = pos + pivot;
        }

        // Also rotate the cull direction to match vertex transformation
        // ============================================================================
        // CRITICAL: cullDirection must rotate in the SAME direction as vertices
        // ============================================================================
        //
        // Vertex rotation uses:
        //   AppendZRotation(-rotY)  - rotates vertices by -rotY degrees (Y rotation)
        //   AppendXRotation(-rotX)  - rotates vertices by -rotX degrees (X rotation)
        //
        // RotateDirection must use the SAME angles to stay synchronized:
        //   RotateDirection(dir, -rotX, -rotY)
        //
        // If we use different angles, the cullDirection rotates opposite to geometry,
        // causing incorrect face culling (e.g., faces that should be visible get
        // culled because their cullDirection points the wrong way).
        //
        // Example: half=top stairs with x:180, y:270
        //   - Vertices rotate by (-180, -270) → geometry flips and rotates
        //   - CullDirection must also rotate by (-180, -270) to match
        //   - If cullDirection uses different angles, NORTH face would wrongly
        //     point in wrong direction, causing incorrect culling
        // ============================================================================
        if (rotY != 0 || rotX != 0)
        {
            face.cullDirection = RotateDirection(face.cullDirection, -rotX, -rotY);
        }
    }

    // Invalidate GPU cache since mesh changed
    gpuDataValid = false;
}
