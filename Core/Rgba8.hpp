#pragma once

struct Rgba8
{
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
    // The constructor Rgba8 is declared as explicit, which means that when creating an Rgba8 object, 
    // the constructor must be explicitly called, as shown below:
    // Rgba8 m_color(255, 0, 0, 255); explicit constructor call
    static const Rgba8 RED;
    static const Rgba8 GREEN;
    static const Rgba8 YELLOW;
    static const Rgba8 BLUE;
    static const Rgba8 WHITE;
    static const Rgba8 GRAY;
    static const Rgba8 ORANGE;
    static const Rgba8 MAGENTA;
    static const Rgba8 CYAN;
    static const Rgba8 DEBUG_BLUE;
    static const Rgba8 DEBUG_WHITE_TRANSLUCENT;
    // If the constructor is not declared with the explicit keyword, 
    // then the object can be created through implicit conversion or copy initialization, as shown below:
    // Rgba8 m_color = Rgba8(255, 0, 0, 255); implicit conversion or copy initialization
    explicit Rgba8(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
    explicit Rgba8();
    Rgba8(const Rgba8& copyFrom);

    friend bool operator==(const Rgba8& lhs, const Rgba8& rhs);
    
    Rgba8 operator*(float multiplier);
    friend bool operator!=(const Rgba8& lhs, const Rgba8& rhs);

    /// Example usage:
    /// Rgba8 m_color;
    /// m_color.SetFromText("255, 128, 40, 100");
    /// @param text String that represent Rgba
    void SetFromText(const char* text);

    /// Convert the r, g, b, and a variables from integers in the range (0, 255) to
    /// floats in the range (0.0f, 1.0f) and place these values in the colorAsFloats array
    /// in the order rgba
    /// @param colorAsFloats array of 4 floats.
    void GetAsFloats(float* colorAsFloats) const;
    
};

Rgba8 Interpolate(Rgba8 from, Rgba8 to, float fractionOfEnd);
