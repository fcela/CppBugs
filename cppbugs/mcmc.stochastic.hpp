///////////////////////////////////////////////////////////////////////////
// Copyright (C) 2010 Whit Armstrong                                     //
//                                                                       //
// This program is free software: you can redistribute it and/or modify  //
// it under the terms of the GNU General Public License as published by  //
// the Free Software Foundation, either version 3 of the License, or     //
// (at your option) any later version.                                   //
//                                                                       //
// This program is distributed in the hope that it will be useful,       //
// but WITHOUT ANY WARRANTY; without even the implied warranty of        //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
// GNU General Public License for more details.                          //
//                                                                       //
// You should have received a copy of the GNU General Public License     //
// along with this program.  If not, see <http://www.gnu.org/licenses/>. //
///////////////////////////////////////////////////////////////////////////

#ifndef MCMC_STOCHASTIC_HPP
#define MCMC_STOCHASTIC_HPP

#include <cppbugs/mcmc.specialized.hpp>

namespace cppbugs {
  double accu(const double x) {
    return x;
  }

  double tune_factor(const double acceptance_ratio) {
    const double univariate_target_ar = 0.7;
    const double thresh = 0.1;
    const double dilution = 0.2;
    double diff = acceptance_ratio - univariate_target_ar;
    return 1.0 + diff * dilution * static_cast<double>(fabs(diff) > thresh);
  }

  template<typename T>
  class Stochastic : public MCMCSpecialized<T> {
  protected:
    bool observed_;
    T accepted_;
    T rejected_;
    T scale_;
  public:
    Stochastic(const T& value, const bool observed): MCMCSpecialized<T>(value), observed_(observed),
						     accepted_(value), rejected_(value),
						     scale_(value)
    {
      accepted_.fill(0);
      rejected_.fill(0);
      scale_.fill(0.25);
    }
    bool isDeterministc() const { return false; }
    bool isStochastic() const { return true; }
    bool isObserved() const { return observed_; }
    void jump(RngBase& rng) {
      if(observed_) {
        return;
      } else {
        for(size_t i = 0; i < MCMCSpecialized<T>::value.n_elem; i++) {
          MCMCSpecialized<T>::value[i] += rng.normal() * scale_[i];
        }
      }
    }

    void component_jump(RngBase& rng, MCModelBase& m) {
      for(size_t i = 0; i < MCMCSpecialized<T>::value.n_elem; i++) {
        double old_logp = m.logp();

        //preserve
        MCMCSpecialized<T>::old_value[i] = MCMCSpecialized<T>::value[i];
        //std::cout << "old:" << MCMCSpecialized<T>::old_value[i] << std::endl;
        // jump
        MCMCSpecialized<T>::value[i] += rng.normal() * scale_[i];
        //std::cout << "new:" << MCMCSpecialized<T>::value[i] << std::endl;

        // update
        m.update();

        //std::cout << "old logp:" << old_logp << std::endl;
        //std::cout << "new logp:" << m.logp() << std::endl;

        // test
        if(m.reject(m.logp(), old_logp)) {
          // revert
          MCMCSpecialized<T>::value[i] = MCMCSpecialized<T>::old_value[i];
          rejected_[i] += 1;
          //std::cout << "rejected" << std::endl;
        } else {
          accepted_[i] += 1;
          //std::cout << "accepted" << std::endl;
        }
      }
    }

    void tune() {
      T ar_ratio = accepted_ / (accepted_ + rejected_);
      for(size_t i = 0; i < MCMCSpecialized<T>::value.n_elem; i++) {
	//std::cout << "[" << i << "]" << ar_ratio[i] << "|" << scale_[i] << "|";
	scale_[i] *= tune_factor(ar_ratio[i]);
	//std::cout << scale_[i] << "|" << tune_factor(ar_ratio[i]) << std::endl;
      }
      //std::cout << "ar_ratio:" << std::endl << ar_ratio;
      //std::cout << "scale:" << std::endl << scale_;
      accepted_.fill(0);
      rejected_.fill(0);
    }
  };

  template<>
  class Stochastic<double> : public MCMCSpecialized<double> {
  protected:
    bool observed_;
    double accepted_,rejected_,scale_;
  public:
    Stochastic(const double& value, const bool observed): MCMCSpecialized<double>(value), observed_(observed),
						     accepted_(0), rejected_(0),
						     scale_(0.25) {}
    bool isDeterministc() const { return false; }
    bool isStochastic() const { return true; }
    bool isObserved() const { return observed_; }
    void jump(RngBase& rng) {
      MCMCSpecialized<double>::value += rng.normal() * scale_;
    }
    void component_jump(RngBase& rng, MCModelBase& m) {
      double old_logp = m.logp();
      MCMCSpecialized<double>::old_value = MCMCSpecialized<double>::value;
      MCMCSpecialized<double>::value += rng.normal() * scale_;
      m.update();
      if(m.reject(m.logp(), old_logp)) {
        MCMCSpecialized<double>::value = MCMCSpecialized<double>::old_value;
        rejected_ += 1;
      } else {
        accepted_ += 1;
      }
    }
    void tune() {
      double ar_ratio = accepted_ / (accepted_ + rejected_);
      scale_ *= tune_factor(ar_ratio);
      //std::cout << "ar_ratio:" << ar_ratio << std::endl;
      //std::cout << "scale:" << scale_ << std::endl;
      accepted_ = 0;
      rejected_ = 0;
    }
  };
} // namespace cppbugs
#endif // MCMC_STOCHASTIC_HPP