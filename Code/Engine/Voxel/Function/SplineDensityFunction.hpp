#pragma once
#include "DensityFunction.hpp"
using namespace enigma::core;

namespace enigma::voxel
{
    class SplineDensityFunction : public DensityFunction
    {
    public:
        struct SplinePoint
        {
            float location; //X coordinate (on spline graph)
            float value; // Y coordinate (simple value)
            float derivative; // slope of this point

            // Nested spline support
            std::unique_ptr<SplineDensityFunction> nestedSpline;

            SplinePoint();
            ~SplinePoint();

            bool IsNested() const
            {
                return nestedSpline != nullptr;
            }

            // Get the actual value of the point (may require recursively evaluating nested splines)
            float GetValue(float coordinateValue) const
            {
                if (IsNested())
                {
                    // Recursion: use coordinateValue to evaluate nested splines
                    return nestedSpline->EvaluateSpline(coordinateValue);
                }
                return value;
            }
        };

    private:
        std::unique_ptr<DensityFunction> m_coordinateFunction; //Input source
        std::vector<SplinePoint>         m_points; // Control point list
        float                            m_minValue; // Output range limit
        float                            m_maxValue; // Output range limit

    public:
        SplineDensityFunction(std::unique_ptr<DensityFunction> coordinateFunction, const std::vector<SplinePoint>& points, float minValue = -1000000.0f, float maxValue = 1000000.0f);

        float Evaluate(int x, int y, int z) const override;

        float EvaluateSpline(float coordinate) const;

        std::string GetTypeName() const override;

        Json ToJson() const;

        // TODO: register to system
        // static std::unique_ptr<SplineDensityFunction> FromJson(const Json& json, DensityFunctionRegistry& registry  // 用于查找其他函数)

    private:
        int   FindSegmentIndex(float coordinate) const;
        float EvaluateSegment(int segmentIndex, float coordinate) const;
        void  SortAndValidatePoints();
    };
}
