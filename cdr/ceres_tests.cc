// Check that ceres works correctly
#include <ceres/ceres.h>
#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <vector>

template <typename T>
T CalculateSABRVol(const T& alpha, const T& rho, const T& nu, double F, double K, double T_exp) {
    using std::abs;
    using std::log;
    using std::pow;
    using std::sqrt;

    const double beta = 1.0;
    const double eps = 1e-7;

    if (abs(F - K) < eps) {
        T sub_1 = T((1.0 - beta) * (1.0 - beta) / 24.0) * alpha * alpha / T(pow(F, 2.0 - 2.0 * beta));
        T sub_2 = T(0.25 * rho * beta * nu) * alpha / T(pow(F, 1.0 - beta));
        T sub_3 = (T(2.0) - T(3.0) * rho * rho) / T(24.0) * nu * nu;
        return (alpha / T(pow(F, 1.0 - beta))) * (T(1.0) + (sub_1 + sub_2 + sub_3) * T(T_exp));
    } else {
        T logFK = T(log(F / K));  // Явное приведение double -> T
        T f_k_beta = T(pow(F * K, (1.0 - beta) / 2.0));

        T z = (nu / alpha) * f_k_beta * logFK;
        T x_z = log((sqrt(T(1.0) - T(2.0) * rho * z + z * z) + z - rho) / (T(1.0) - rho));

        T numer_1 = T((1.0 - beta) * (1.0 - beta) / 24.0) * alpha * alpha / pow(f_k_beta, 2.0);
        T numer_2 = T(0.25) * rho * T(beta) * nu * alpha / f_k_beta;
        T numer_3 = (T(2.0) - T(3.0) * rho * rho) / T(24.0) * nu * nu;

        T denum_1 = T((1.0 - beta) * (1.0 - beta) / 24.0) * logFK * logFK;
        T denum_2 = T(pow((1.0 - beta), 4.0) / 1920.0) * pow(logFK, 4.0);

        return (alpha / (f_k_beta * (T(1.0) + denum_1 + denum_2))) * (z / x_z) *
               (T(1.0) + (numer_1 + numer_2 + numer_3) * T(T_exp));
    }
}

// 2. Функтор для Ceres
struct SabrCostFunctor {
    SabrCostFunctor(double market_vol, double F, double K, double T_exp)
        : m_vol(market_vol), m_F(F), m_K(K), m_T(T_exp) {
    }

    template <typename T>
    bool operator()(const T* const alpha, const T* const rho, const T* const nu, T* residual) const {
        // Вычисляем волатильность по модели
        T model_vol = CalculateSABRVol(alpha[0], rho[0], nu[0], m_F, m_K, m_T);
        // Ошибка (residual)
        residual[0] = model_vol - T(m_vol);
        return true;
    }

    const double m_vol, m_F, m_K, m_T;
};

TEST(SABRProvider, CalibrationWithJet) {
    const double F = 100.0;
    const double K = 105.0;  // Немного вне денег
    const double T = 1.0;
    const double market_vol = 0.22;

    double alpha = 0.2;
    double rho = -0.2;
    double nu = 0.4;

    ceres::Problem problem;

    problem.AddResidualBlock(
        new ceres::AutoDiffCostFunction<SabrCostFunctor, 1, 1, 1, 1>(new SabrCostFunctor(market_vol, F, K, T)), nullptr,
        &alpha, &rho, &nu);

    problem.SetParameterLowerBound(&alpha, 0, 0.001);
    problem.SetParameterLowerBound(&rho, 0, -0.99);
    problem.SetParameterUpperBound(&rho, 0, 0.99);
    problem.SetParameterLowerBound(&nu, 0, 0.001);

    ceres::Solver::Options options;
    options.max_num_iterations = 50;
    options.linear_solver_type = ceres::DENSE_QR;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    std::cout << "Solver report:\n" << summary.BriefReport() << "\n";
    std::cout << "Final params: a=" << alpha << ", r=" << rho << ", n=" << nu << "\n";

    EXPECT_TRUE(summary.IsSolutionUsable());
    double final_vol = CalculateSABRVol(alpha, rho, nu, F, K, T);
    EXPECT_NEAR(final_vol, market_vol, 1e-6);
}
