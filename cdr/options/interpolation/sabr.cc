#include <cdr/options/interpolation/sabr.h>
#include <ceres/ceres.h>

namespace cdr {

Expect<void, Error> SABRInterpolator::InitState(void* coefs_ptr, std::span<const f64> xs,
                                                std::span<const f64> ys) noexcept {
    auto* p = static_cast<SabrParameters*>(coefs_ptr);

    // Начальное приближение (Initial Guess)
    // alpha берем как ATM волатильность (примерно посередине вектора ys)
    double params[3] = {ys[ys.size() / 2], 0.0, 0.5};

    ceres::Problem problem;
    for (size_t i = 0; i < xs.size(); ++i) {
        auto* cost_function =
            new ceres::AutoDiffCostFunction<SabrCostFunctor, 1, 3>(new SabrCostFunctor(xs[i], ys[i], p->F, p->T));
        problem.AddResidualBlock(cost_function, nullptr, params);
    }

    // Накладываем ограничения, чтобы параметры не улетели в космос
    problem.SetParameterLowerBound(params, 0, 1e-6);   // alpha > 0
    problem.SetParameterLowerBound(params, 1, -0.99);  // rho > -1
    problem.SetParameterUpperBound(params, 1, 0.99);   // rho < 1
    problem.SetParameterLowerBound(params, 2, 1e-6);   // nu > 0

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_QR;
    options.max_num_iterations = 50;
    // Levenberg-Marquardt по умолчанию в Ceres

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    if (!summary.IsSolutionUsable()) {
        return ErrorCalibrationFailed();
    }

    p->alpha = params[0];
    p->rho = params[1];
    p->nu = params[2];

    return Ok();
}

[[nodiscard]] Expect<f64, Error> SABRInterpolator::Evaluate(const void* coefs_ptr,
                                                            [[maybe_unused]] std::span<const f64> xs, f64 K) noexcept {
    const auto& p = *static_cast<const SabrParameters*>(coefs_ptr);
    return Ok(CalculateHagan<f64>(K, p.F, p.T, p.alpha, p.rho, p.nu));
}

}  // namespace cdr
