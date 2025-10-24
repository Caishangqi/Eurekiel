#include "SplineDensityFunction.hpp"

#include "Engine/Math/MathUtils.hpp"
using namespace enigma::voxel;

SplineDensityFunction::SplineDensityFunction(std::unique_ptr<DensityFunction> coordinateFunction, const std::vector<SplinePoint>& points, float minValue, float maxValue) :
    m_coordinateFunction(std::move(coordinateFunction)), m_points(points), m_minValue(minValue), m_maxValue(maxValue)
{
}

float SplineDensityFunction::Evaluate(int x, int y, int z) const
{
    // Step 1: Evaluate the coordinate function and get the input of spline
    float coordinateValue = m_coordinateFunction->Evaluate(x, y, z);

    // Step 2: Use this value to interpolate on the spline
    float result = EvaluateSpline(coordinateValue);

    // Step 3: Limit to min/max range
    result = GetClamped(result, m_minValue, m_maxValue);
    return result;
}

float SplineDensityFunction::EvaluateSpline(float coordinate) const
{
    //Boundary case 1: coordinate is less than the minimum location
    if (coordinate <= m_points.front().location)
    {
        return m_points.front().GetValue(coordinate);
    }

    // Boundary case 2: coordinate is greater than the maximum location
    if (coordinate >= m_points.back().location)
    {
        return m_points.back().GetValue(coordinate);
    }

    // Step 1: Find the interval where coordinate is located [points[i], points[i+1]]
    int segmentIndex = FindSegmentIndex(coordinate);

    // Step 2: Perform Hermite interpolation within this interval
    return EvaluateSegment(segmentIndex, coordinate);
}

int SplineDensityFunction::FindSegmentIndex(float coordinate) const
{
    int left  = 0;
    int right = (int)m_points.size() - 2;

    while (left < right)
    {
        int mid = (left + right) / 2;
        if (coordinate < m_points[mid + 1].location)
        {
            right = mid;
        }
        else
        {
            left = mid + 1;
        }
    }
    return left;
}

float SplineDensityFunction::EvaluateSegment(int segmentIndex, float coordinate) const
{
    const SplinePoint& p0 = m_points[segmentIndex];
    const SplinePoint& p1 = m_points[segmentIndex + 1];

    // Step 1: Calculate the normalization parameter t ∈ [0, 1]
    float x0 = p0.location;
    float x1 = p1.location;
    float t  = (coordinate - x0) / (x1 - x0); // linear mapping to [0,1]

    // Step 2: Get the value of the endpoint (may require recursively evaluating nested splines)
    float v0 = p0.GetValue(coordinate);
    float v1 = p1.GetValue(coordinate);
    float d0 = p0.derivative;
    float d1 = p1.derivative;

    // Step 3: Hermite basis function
    float t2 = t * t;
    float t3 = t2 * t;

    float h00 = 2 * t3 - 3 * t2 + 1; // (2t³ - 3t² + 1)
    float h10 = t3 - 2 * t2 + t; // (t³ - 2t² + t)
    float h01 = -2 * t3 + 3 * t2; // (-2t³ + 3t²)
    float h11 = t3 - t2; // (t³ - t²)

    // Step 4: Hermite interpolation formula
    // Note: derivative needs to be multiplied by the interval length
    float dx     = x1 - x0;
    float result = h00 * v0 +
        h10 * d0 * dx +
        h01 * v1 +
        h11 * d1 * dx;

    return result;
}

void SplineDensityFunction::SortAndValidatePoints()
{
    // Sort by location
    std::sort(m_points.begin(), m_points.end(),
              [](const SplinePoint& a, const SplinePoint& b)
              {
                  return a.location < b.location;
              });

    // Verification: at least 1 point required
    if (m_points.empty())
    {
        throw std::runtime_error("Spline must have at least 1 point");
    }

    // Verification: location cannot be repeated
    for (size_t i = 1; i < m_points.size(); i++)
    {
        if (m_points[i].location == m_points[i - 1].location)
        {
            throw std::runtime_error("Duplicate spline locations");
        }
    }
}

std::string SplineDensityFunction::GetTypeName() const
{
    return "engine:spline";
}

Json SplineDensityFunction::ToJson() const
{
    Json json;
    json["type"] = "engine:spline";
    //json["coordinate"] = m_coordinateFunction->GetRegistryId();

    Json pointsArray = Json::array();
    for (const auto& point : m_points)
    {
        Json pointJson;
        pointJson["location"]   = point.location;
        pointJson["derivative"] = point.derivative;

        if (point.IsNested())
        {
            pointJson["value"] = point.nestedSpline->ToJson();
        }
        else
        {
            pointJson["value"] = point.value;
        }

        pointsArray.push_back(pointJson);
    }
    json["points"] = pointsArray;

    json["min_value"] = m_minValue;
    json["max_value"] = m_maxValue;

    return json;
}
