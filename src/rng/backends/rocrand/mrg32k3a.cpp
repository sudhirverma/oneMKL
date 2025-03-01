/*******************************************************************************
 * Copyright (C) 2022 Heidelberg University, Engineering Mathematics and Computing Lab (EMCL) 
 * and Computing Centre (URZ)
 * cuRAND back-end Copyright (c) 2021, The Regents of the University of
 * California, through Lawrence Berkeley National Laboratory (subject to receipt
 * of any required approvals from the U.S. Dept. of Energy). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * (1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * (2) Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * (3) Neither the name of the University of California, Lawrence Berkeley
 * National Laboratory, U.S. Dept. of Energy nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * You are under no obligation whatsoever to provide any bug fixes, patches,
 * or upgrades to the features, functionality or performance of the source
 * code ("Enhancements") to anyone; however, if you choose to make your
 * Enhancements available either publicly, or directly to Lawrence Berkeley
 * National Laboratory, without imposing a separate written license agreement
 * for such Enhancements, then you hereby grant the following license: a
 * non-exclusive, royalty-free perpetual license to install, use, modify,
 * prepare derivative works, incorporate into other computer software,
 * distribute, and sublicense such enhancements or derivative works thereof,
 * in binary and source code form.
 *
 * If you have questions about your rights to use or distribute this software,
 * please contact Berkeley Lab's Intellectual Property Office at
 * IPO@lbl.gov.
 *
 * NOTICE.  This Software was developed under funding from the U.S. Department
 * of Energy and the U.S. Government consequently retains certain rights.  As
 * such, the U.S. Government has been granted for itself and others acting on
 * its behalf a paid-up, nonexclusive, irrevocable, worldwide license in the
 * Software to reproduce, distribute copies to the public, prepare derivative
 * works, and perform publicly and display publicly, and to permit others to do
 * so.
 ******************************************************************************/

#if __has_include(<sycl/sycl.hpp>)
#include <sycl/sycl.hpp>
#else
#include <CL/sycl.hpp>
#endif
#ifndef __HIPSYCL__
#if __has_include(<sycl/backend/cuda.hpp>)
#include <sycl/backend/cuda.hpp>
#else
#include <CL/sycl/backend/cuda.hpp>
#endif
#endif
#include <iostream>

#include "rocrand_helper.hpp"
#include "rocrand_task.hpp"
#include "oneapi/mkl/exceptions.hpp"
#include "oneapi/mkl/rng/detail/rocrand/onemkl_rng_rocrand.hpp"
#include "oneapi/mkl/rng/detail/engine_impl.hpp"
#include "oneapi/mkl/rng/engines.hpp"

namespace oneapi {
namespace mkl {
namespace rng {
namespace rocrand {

#if !defined(_WIN64)
class mrg32k3a_impl : public oneapi::mkl::rng::detail::engine_impl {
public:
    mrg32k3a_impl(sycl::queue queue, std::uint32_t seed)
            : oneapi::mkl::rng::detail::engine_impl(queue) {
        rocrand_status status;
        ROCRAND_CALL(rocrand_create_generator, status, &engine_, ROCRAND_RNG_PSEUDO_MRG32K3A);
        ROCRAND_CALL(rocrand_set_seed, status, engine_, (unsigned long long)seed);
    }

    mrg32k3a_impl(sycl::queue queue, std::initializer_list<std::uint32_t> seed)
            : oneapi::mkl::rng::detail::engine_impl(queue) {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine",
                                         "multi-seed unsupported by cuRAND backend");
    }

    mrg32k3a_impl(const mrg32k3a_impl* other) : oneapi::mkl::rng::detail::engine_impl(*other) {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine",
                                         "copy construction unsupported by cuRAND backend");
    }

    // Buffers API

    virtual void generate(
        const oneapi::mkl::rng::uniform<float, oneapi::mkl::rng::uniform_method::standard>& distr,
        std::int64_t n, sycl::buffer<float, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](float* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_uniform, status, engine_, r_ptr, n);
                });
            })
            .wait_and_throw();
        range_transform_fp<float>(queue_, distr.a(), distr.b(), n, r);
    }

    virtual void generate(
        const oneapi::mkl::rng::uniform<double, oneapi::mkl::rng::uniform_method::standard>& distr,
        std::int64_t n, sycl::buffer<double, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](double* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_uniform_double, status, engine_, r_ptr, n);
                });
            })
            .wait_and_throw();
        range_transform_fp<double>(queue_, distr.a(), distr.b(), n, r);
    }

    virtual void generate(const oneapi::mkl::rng::uniform<
                              std::int32_t, oneapi::mkl::rng::uniform_method::standard>& distr,
                          std::int64_t n, sycl::buffer<std::int32_t, 1>& r) override {
        sycl::buffer<std::uint32_t, 1> ib(n);
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = ib.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](std::uint32_t* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate, status, engine_, r_ptr, n);
                });
            })
            .wait_and_throw();
        range_transform_int<std::int32_t>(queue_, distr.a(), distr.b(), n, ib, r);
    }

    virtual void generate(
        const oneapi::mkl::rng::uniform<float, oneapi::mkl::rng::uniform_method::accurate>& distr,
        std::int64_t n, sycl::buffer<float, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](float* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_uniform, status, engine_, r_ptr, n);
                });
            })
            .wait_and_throw();
        range_transform_fp_accurate<float>(queue_, distr.a(), distr.b(), n, r);
    }

    virtual void generate(
        const oneapi::mkl::rng::uniform<double, oneapi::mkl::rng::uniform_method::accurate>& distr,
        std::int64_t n, sycl::buffer<double, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](double* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_uniform_double, status, engine_, r_ptr, n);
                });
            })
            .wait_and_throw();
        range_transform_fp_accurate<double>(queue_, distr.a(), distr.b(), n, r);
    }

    virtual void generate(const oneapi::mkl::rng::gaussian<
                              float, oneapi::mkl::rng::gaussian_method::box_muller2>& distr,
                          std::int64_t n, sycl::buffer<float, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](float* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_normal, status, engine_, r_ptr, n, distr.mean(),
                                 distr.stddev());
                });
            })
            .wait_and_throw();
    }

    virtual void generate(const oneapi::mkl::rng::gaussian<
                              double, oneapi::mkl::rng::gaussian_method::box_muller2>& distr,
                          std::int64_t n, sycl::buffer<double, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](double* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_normal_double, status, engine_, r_ptr, n,
                                 distr.mean(), distr.stddev());
                });
            })
            .wait_and_throw();
    }

    virtual void generate(
        const oneapi::mkl::rng::gaussian<float, oneapi::mkl::rng::gaussian_method::icdf>& distr,
        std::int64_t n, sycl::buffer<float, 1>& r) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
    }

    virtual void generate(
        const oneapi::mkl::rng::gaussian<double, oneapi::mkl::rng::gaussian_method::icdf>& distr,
        std::int64_t n, sycl::buffer<double, 1>& r) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
    }

    virtual void generate(const oneapi::mkl::rng::lognormal<
                              float, oneapi::mkl::rng::lognormal_method::box_muller2>& distr,
                          std::int64_t n, sycl::buffer<float, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](float* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_log_normal, status, engine_, r_ptr, n, distr.m(),
                                 distr.s());
                });
            })
            .wait_and_throw();
    }

    virtual void generate(const oneapi::mkl::rng::lognormal<
                              double, oneapi::mkl::rng::lognormal_method::box_muller2>& distr,
                          std::int64_t n, sycl::buffer<double, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](double* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_log_normal_double, status, engine_, r_ptr, n,
                                 distr.m(), distr.s());
                });
            })
            .wait_and_throw();
    }

    virtual void generate(
        const oneapi::mkl::rng::lognormal<float, oneapi::mkl::rng::lognormal_method::icdf>& distr,
        std::int64_t n, sycl::buffer<float, 1>& r) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
    }

    virtual void generate(
        const oneapi::mkl::rng::lognormal<double, oneapi::mkl::rng::lognormal_method::icdf>& distr,
        std::int64_t n, sycl::buffer<double, 1>& r) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
    }

    virtual void generate(const bernoulli<std::int32_t, bernoulli_method::icdf>& distr,
                          std::int64_t n, sycl::buffer<std::int32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
    }

    virtual void generate(const bernoulli<std::uint32_t, bernoulli_method::icdf>& distr,
                          std::int64_t n, sycl::buffer<std::uint32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
    }

    virtual void generate(const poisson<std::int32_t, poisson_method::gaussian_icdf_based>& distr,
                          std::int64_t n, sycl::buffer<std::int32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
    }

    virtual void generate(const poisson<std::uint32_t, poisson_method::gaussian_icdf_based>& distr,
                          std::int64_t n, sycl::buffer<std::uint32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
    }

    virtual void generate(const bits<std::uint32_t>& distr, std::int64_t n,
                          sycl::buffer<std::uint32_t, 1>& r) override {
        queue_
            .submit([&](sycl::handler& cgh) {
                auto acc = r.template get_access<sycl::access::mode::read_write>(cgh);
                onemkl_rocrand_host_task(cgh, acc, engine_, [=](uint32_t* r_ptr) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate, status, engine_, r_ptr, n);
                });
            })
            .wait_and_throw();
    }

    // USM APIs

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<float, oneapi::mkl::rng::uniform_method::standard>& distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        queue_
            .submit([&](sycl::handler& cgh) {
                onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_uniform, status, engine_, r, n);
                });
            })
            .wait_and_throw();
        return range_transform_fp<float>(queue_, distr.a(), distr.b(), n, r);
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<double, oneapi::mkl::rng::uniform_method::standard>& distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        queue_
            .submit([&](sycl::handler& cgh) {
                onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_uniform_double, status, engine_, r, n);
                });
            })
            .wait_and_throw();
        return range_transform_fp<double>(queue_, distr.a(), distr.b(), n, r);
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<std::int32_t, oneapi::mkl::rng::uniform_method::standard>&
            distr,
        std::int64_t n, std::int32_t* r, const std::vector<sycl::event>& dependencies) override {
        std::uint32_t* ib = (std::uint32_t*)malloc_device(
            n * sizeof(std::uint32_t), queue_.get_device(), queue_.get_context());
        queue_
            .submit([&](sycl::handler& cgh) {
                onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate, status, engine_, ib, n);
                });
            })
            .wait_and_throw();
        return range_transform_int(queue_, distr.a(), distr.b(), n, ib, r);
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<float, oneapi::mkl::rng::uniform_method::accurate>& distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        queue_
            .submit([&](sycl::handler& cgh) {
                onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_uniform, status, engine_, r, n);
                });
            })
            .wait_and_throw();
        return range_transform_fp_accurate<float>(queue_, distr.a(), distr.b(), n, r);
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<double, oneapi::mkl::rng::uniform_method::accurate>& distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        queue_
            .submit([&](sycl::handler& cgh) {
                onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                    rocrand_status status;
                    ROCRAND_CALL(rocrand_generate_uniform_double, status, engine_, r, n);
                });
            })
            .wait_and_throw();
        return range_transform_fp_accurate<double>(queue_, distr.a(), distr.b(), n, r);
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::gaussian<float, oneapi::mkl::rng::gaussian_method::box_muller2>&
            distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        return queue_.submit([&](sycl::handler& cgh) {
            onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                rocrand_status status;
                ROCRAND_CALL(rocrand_generate_normal, status, engine_, r, n, distr.mean(),
                             distr.stddev());
            });
        });
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::gaussian<double, oneapi::mkl::rng::gaussian_method::box_muller2>&
            distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        return queue_.submit([&](sycl::handler& cgh) {
            onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                rocrand_status status;
                ROCRAND_CALL(rocrand_generate_normal_double, status, engine_, r, n, distr.mean(),
                             distr.stddev());
            });
        });
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::gaussian<float, oneapi::mkl::rng::gaussian_method::icdf>& distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::gaussian<double, oneapi::mkl::rng::gaussian_method::icdf>& distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::lognormal<float, oneapi::mkl::rng::lognormal_method::box_muller2>&
            distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        return queue_.submit([&](sycl::handler& cgh) {
            onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                rocrand_status status;
                ROCRAND_CALL(rocrand_generate_log_normal, status, engine_, r, n, distr.m(),
                             distr.s());
            });
        });
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::lognormal<double, oneapi::mkl::rng::lognormal_method::box_muller2>&
            distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        return queue_.submit([&](sycl::handler& cgh) {
            onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                rocrand_status status;
                ROCRAND_CALL(rocrand_generate_log_normal_double, status, engine_, r, n, distr.m(),
                             distr.s());
            });
        });
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::lognormal<float, oneapi::mkl::rng::lognormal_method::icdf>& distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::lognormal<double, oneapi::mkl::rng::lognormal_method::icdf>& distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
        return sycl::event{};
    }

    virtual sycl::event generate(const bernoulli<std::int32_t, bernoulli_method::icdf>& distr,
                                 std::int64_t n, std::int32_t* r,
                                 const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
        return sycl::event{};
    }

    virtual sycl::event generate(const bernoulli<std::uint32_t, bernoulli_method::icdf>& distr,
                                 std::int64_t n, std::uint32_t* r,
                                 const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const poisson<std::int32_t, poisson_method::gaussian_icdf_based>& distr, std::int64_t n,
        std::int32_t* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const poisson<std::uint32_t, poisson_method::gaussian_icdf_based>& distr, std::int64_t n,
        std::uint32_t* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented(
            "rng", "mrg32ka engine",
            "ICDF method not used for pseudorandom generators in cuRAND backend");
        return sycl::event{};
    }

    virtual sycl::event generate(const bits<std::uint32_t>& distr, std::int64_t n, std::uint32_t* r,
                                 const std::vector<sycl::event>& dependencies) override {
        sycl::event::wait_and_throw(dependencies);
        return queue_.submit([&](sycl::handler& cgh) {
            onemkl_rocrand_host_task(cgh, engine_, [=](sycl::interop_handle ih) {
                rocrand_status status;
                ROCRAND_CALL(rocrand_generate, status, engine_, r, n);
            });
        });
    }

    virtual oneapi::mkl::rng::detail::engine_impl* copy_state() override {
        return new mrg32k3a_impl(this);
    }

    virtual void skip_ahead(std::uint64_t num_to_skip) override {
        rocrand_status status;
        ROCRAND_CALL(rocrand_set_offset, status, engine_, num_to_skip);
    }

    virtual void skip_ahead(std::initializer_list<std::uint64_t> num_to_skip) override {
        throw oneapi::mkl::unimplemented("rng", "skip_ahead",
                                         "initializer list unsupported by cuRAND backend");
    }

    virtual void leapfrog(std::uint64_t idx, std::uint64_t stride) override {
        throw oneapi::mkl::unimplemented("rng", "leapfrog", "unsupported by cuRAND backend");
    }

    virtual ~mrg32k3a_impl() override {
        rocrand_destroy_generator(engine_);
    }

private:
    rocrand_generator engine_;
    std::uint32_t seed_;
};
#else // cuRAND backend is currently not supported on Windows
class mrg32k3a_impl : public oneapi::mkl::rng::detail::engine_impl {
public:
    mrg32k3a_impl(sycl::queue queue, std::uint32_t seed)
            : oneapi::mkl::rng::detail::engine_impl(queue) {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    mrg32k3a_impl(sycl::queue queue, std::initializer_list<std::uint32_t> seed)
            : oneapi::mkl::rng::detail::engine_impl(queue) {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    mrg32k3a_impl(const mrg32k3a_impl* other) : oneapi::mkl::rng::detail::engine_impl(*other) {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    // Buffers API

    virtual void generate(
        const oneapi::mkl::rng::uniform<float, oneapi::mkl::rng::uniform_method::standard>& distr,
        std::int64_t n, sycl::buffer<float, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(
        const oneapi::mkl::rng::uniform<double, oneapi::mkl::rng::uniform_method::standard>& distr,
        std::int64_t n, sycl::buffer<double, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const oneapi::mkl::rng::uniform<
                              std::int32_t, oneapi::mkl::rng::uniform_method::standard>& distr,
                          std::int64_t n, sycl::buffer<std::int32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(
        const oneapi::mkl::rng::uniform<float, oneapi::mkl::rng::uniform_method::accurate>& distr,
        std::int64_t n, sycl::buffer<float, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(
        const oneapi::mkl::rng::uniform<double, oneapi::mkl::rng::uniform_method::accurate>& distr,
        std::int64_t n, sycl::buffer<double, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const oneapi::mkl::rng::gaussian<
                              float, oneapi::mkl::rng::gaussian_method::box_muller2>& distr,
                          std::int64_t n, sycl::buffer<float, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const oneapi::mkl::rng::gaussian<
                              double, oneapi::mkl::rng::gaussian_method::box_muller2>& distr,
                          std::int64_t n, sycl::buffer<double, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(
        const oneapi::mkl::rng::gaussian<float, oneapi::mkl::rng::gaussian_method::icdf>& distr,
        std::int64_t n, sycl::buffer<float, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(
        const oneapi::mkl::rng::gaussian<double, oneapi::mkl::rng::gaussian_method::icdf>& distr,
        std::int64_t n, sycl::buffer<double, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const oneapi::mkl::rng::lognormal<
                              float, oneapi::mkl::rng::lognormal_method::box_muller2>& distr,
                          std::int64_t n, sycl::buffer<float, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const oneapi::mkl::rng::lognormal<
                              double, oneapi::mkl::rng::lognormal_method::box_muller2>& distr,
                          std::int64_t n, sycl::buffer<double, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(
        const oneapi::mkl::rng::lognormal<float, oneapi::mkl::rng::lognormal_method::icdf>& distr,
        std::int64_t n, sycl::buffer<float, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(
        const oneapi::mkl::rng::lognormal<double, oneapi::mkl::rng::lognormal_method::icdf>& distr,
        std::int64_t n, sycl::buffer<double, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const bernoulli<std::int32_t, bernoulli_method::icdf>& distr,
                          std::int64_t n, sycl::buffer<std::int32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const bernoulli<std::uint32_t, bernoulli_method::icdf>& distr,
                          std::int64_t n, sycl::buffer<std::uint32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const poisson<std::int32_t, poisson_method::gaussian_icdf_based>& distr,
                          std::int64_t n, sycl::buffer<std::int32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const poisson<std::uint32_t, poisson_method::gaussian_icdf_based>& distr,
                          std::int64_t n, sycl::buffer<std::uint32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void generate(const bits<std::uint32_t>& distr, std::int64_t n,
                          sycl::buffer<std::uint32_t, 1>& r) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    // USM APIs

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<float, oneapi::mkl::rng::uniform_method::standard>& distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<double, oneapi::mkl::rng::uniform_method::standard>& distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<std::int32_t, oneapi::mkl::rng::uniform_method::standard>&
            distr,
        std::int64_t n, std::int32_t* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<float, oneapi::mkl::rng::uniform_method::accurate>& distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::uniform<double, oneapi::mkl::rng::uniform_method::accurate>& distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::gaussian<float, oneapi::mkl::rng::gaussian_method::box_muller2>&
            distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::gaussian<double, oneapi::mkl::rng::gaussian_method::box_muller2>&
            distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::gaussian<float, oneapi::mkl::rng::gaussian_method::icdf>& distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::gaussian<double, oneapi::mkl::rng::gaussian_method::icdf>& distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::lognormal<float, oneapi::mkl::rng::lognormal_method::box_muller2>&
            distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::lognormal<double, oneapi::mkl::rng::lognormal_method::box_muller2>&
            distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::lognormal<float, oneapi::mkl::rng::lognormal_method::icdf>& distr,
        std::int64_t n, float* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const oneapi::mkl::rng::lognormal<double, oneapi::mkl::rng::lognormal_method::icdf>& distr,
        std::int64_t n, double* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(const bernoulli<std::int32_t, bernoulli_method::icdf>& distr,
                                 std::int64_t n, std::int32_t* r,
                                 const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(const bernoulli<std::uint32_t, bernoulli_method::icdf>& distr,
                                 std::int64_t n, std::uint32_t* r,
                                 const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const poisson<std::int32_t, poisson_method::gaussian_icdf_based>& distr, std::int64_t n,
        std::int32_t* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(
        const poisson<std::uint32_t, poisson_method::gaussian_icdf_based>& distr, std::int64_t n,
        std::uint32_t* r, const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual sycl::event generate(const bits<std::uint32_t>& distr, std::int64_t n, std::uint32_t* r,
                                 const std::vector<sycl::event>& dependencies) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return sycl::event{};
    }

    virtual oneapi::mkl::rng::detail::engine_impl* copy_state() override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
        return nullptr;
    }

    virtual void skip_ahead(std::uint64_t num_to_skip) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void skip_ahead(std::initializer_list<std::uint64_t> num_to_skip) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual void leapfrog(std::uint64_t idx, std::uint64_t stride) override {
        throw oneapi::mkl::unimplemented("rng", "mrg32ka engine");
    }

    virtual ~mrg32k3a_impl() override {}
};
#endif

oneapi::mkl::rng::detail::engine_impl* create_mrg32k3a(sycl::queue queue, std::uint32_t seed) {
    return new mrg32k3a_impl(queue, seed);
}

oneapi::mkl::rng::detail::engine_impl* create_mrg32k3a(sycl::queue queue,
                                                       std::initializer_list<std::uint32_t> seed) {
    return new mrg32k3a_impl(queue, seed);
}

} // namespace rocrand
} // namespace rng
} // namespace mkl
} // namespace oneapi
