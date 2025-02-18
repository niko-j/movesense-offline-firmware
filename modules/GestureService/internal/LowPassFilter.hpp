#include "wb-resources/resources.h"

namespace gesture_svc
{
#ifndef GESTURE_SVC_LOW_PASS_FILTER_HPP
#define GESTURE_SVC_LOW_PASS_FILTER_HPP
    class LowPassFilter
    {
    private:
        wb::FloatVector3D m_b;
        float m_cutoff;

    public:
        LowPassFilter(float cutoff = 0.1f);
        wb::FloatVector3D filter(const wb::FloatVector3D& input);
        void reset();
    };
#endif // GESTURE_SVC_LOW_PASS_FILTER_HPP

#ifdef GESTURE_SVC_LOW_PASS_FILTER_IMPL
#undef GESTURE_SVC_LOW_PASS_FILTER_IMPL
    LowPassFilter::LowPassFilter(float cutoff)
        : m_b(0.0f, 0.0f, 0.0f)
        , m_cutoff(cutoff)
    {
    }

    wb::FloatVector3D LowPassFilter::filter(const wb::FloatVector3D& input)
    {
        m_b.x = (1.0f - m_cutoff) * m_b.x + m_cutoff * input.x;
        m_b.y = (1.0f - m_cutoff) * m_b.y + m_cutoff * input.y;
        m_b.z = (1.0f - m_cutoff) * m_b.z + m_cutoff * input.z;
        return input - m_b;
    }

    void LowPassFilter::reset()
    {
        m_b = wb::FloatVector3D(0.0f, 0.0f, 0.0f);
    }
#endif // ifdef GESTURE_SVC_LOW_PASS_FILTER_IMPL

} // namespace gesture_svc
