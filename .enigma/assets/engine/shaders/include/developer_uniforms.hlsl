// [NEW] Test Custom Uniform for multi-draw data independence verification
// Uses slot 42 with space1 (Custom Buffer path via Descriptor Table)
cbuffer TestUserCustomUniform : register(b42, space1)
{
    float4 color; // RGBA color for visual verification
    float4 padding[3]; // Padding to 64 bytes (256-byte aligned for CBV)
};
