#pragma once

#include <algorithm>
#include <cmath>

namespace CoreDSP::Chaos
{
    /** Classic Lorenz system parameters. The defaults produce the well-known butterfly attractor. */
    struct LorenzParams
    {
        double sigma = 10.0;
        double rho = 28.0;
        double beta = 8.0 / 3.0;
    };

    /** Position of the system in phase space. */
    struct LorenzState
    {
        double x = 0.1;
        double y = 0.0;
        double z = 0.0;
    };

    /**
        Lorenz attractor integrated with a fixed-step RK4 solver.

        On the classic parameters the raw state stays inside roughly
        x in [-20, 20], y in [-30, 30] and z in [0, 50]. The normalised accessors
        map that box onto [-1, 1] so callers can use the values directly as modulators.
    */
    class LorenzAttractor
    {
    public:
        using State = LorenzState;

        /** Largest integration step, in attractor time units. Keeps RK4 well inside its stability region. */
        static constexpr double maxStepSize = 0.005;

        /** Upper bound on a single advance() call, so a large host jump cannot stall the audio thread. */
        static constexpr double maxAdvance = 5.0;

        explicit LorenzAttractor (LorenzParams parameters = {}) noexcept
            : params (parameters) {}

        void reset (State initial = {}) noexcept { state = initial; }

        /** Integrates forward by the given amount of attractor time, clamped to [0, maxAdvance]. */
        void advance (double time) noexcept
        {
            time = std::clamp (time, 0.0, maxAdvance);

            if (time <= 0.0)
                return;

            const int steps = static_cast<int> (std::ceil (time / maxStepSize));
            const double h = time / steps;

            for (int i = 0; i < steps; ++i)
                rk4Step (h);
        }

        const State& getState() const noexcept { return state; }

        /** X scaled to [-1, 1]. */
        float normalisedX() const noexcept { return clampUnit (state.x / 20.0); }

        /** Y scaled to [-1, 1]. */
        float normalisedY() const noexcept { return clampUnit (state.y / 30.0); }

        /** Z scaled to [-1, 1], centred on the attractor's typical value of 25. */
        float normalisedZ() const noexcept { return clampUnit ((state.z - 25.0) / 25.0); }

    private:
        static float clampUnit (double value) noexcept
        {
            return static_cast<float> (std::clamp (value, -1.0, 1.0));
        }

        State derivative (const State& s) const noexcept
        {
            return { params.sigma * (s.y - s.x),
                     s.x * (params.rho - s.z) - s.y,
                     s.x * s.y - params.beta * s.z };
        }

        static State offset (const State& s, const State& d, double scale) noexcept
        {
            return { s.x + d.x * scale, s.y + d.y * scale, s.z + d.z * scale };
        }

        void rk4Step (double h) noexcept
        {
            const State k1 = derivative (state);
            const State k2 = derivative (offset (state, k1, 0.5 * h));
            const State k3 = derivative (offset (state, k2, 0.5 * h));
            const State k4 = derivative (offset (state, k3, h));

            state.x += h / 6.0 * (k1.x + 2.0 * k2.x + 2.0 * k3.x + k4.x);
            state.y += h / 6.0 * (k1.y + 2.0 * k2.y + 2.0 * k3.y + k4.y);
            state.z += h / 6.0 * (k1.z + 2.0 * k2.z + 2.0 * k3.z + k4.z);
        }

        LorenzParams params;
        State state;
    };
}
