#pragma once
#include "wb-resources/resources.h"

namespace offline_meas
{
    enum class FilterType
    {
        LowPass,
        HighPass
    };

    template<FilterType FT>
    class SimpleFilter
    {
    private:
        wb::FloatVector3D m_b;
        float m_cutoff;

    public:
        SimpleFilter(float cutoff = 0.1f)
            : m_b(0.0f, 0.0f, 0.0f)
            , m_cutoff(cutoff)
        {
        }

        wb::FloatVector3D filter(const wb::FloatVector3D& input)
        {
            m_b.x = (1.0f - m_cutoff) * m_b.x + m_cutoff * input.x;
            m_b.y = (1.0f - m_cutoff) * m_b.y + m_cutoff * input.y;
            m_b.z = (1.0f - m_cutoff) * m_b.z + m_cutoff * input.z;
            
            if (FT == FilterType::LowPass)
                return input - m_b;
            else
                return input;
        }

        void reset()
        {
            m_b = wb::FloatVector3D(0.0f, 0.0f, 0.0f);
        }
    };

} // namespace offline_meas
