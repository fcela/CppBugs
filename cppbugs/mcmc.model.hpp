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

#ifndef MCMC_MODEL_HPP
#define MCMC_MODEL_HPP

#include <cmath>
#include <iostream>
#include <vector>
#include <boost/random.hpp>
#include <cppbugs/mcmc.rng.hpp>
#include <cppbugs/mcmc.model.base.hpp>
#include <cppbugs/mcmc.object.hpp>
#include <cppbugs/cppbugs.hpp>

namespace cppbugs {

  class MCModel : public MCModelBase {
  private:
    double accepted_;
    double rejected_;
    SpecializedRng<boost::minstd_rand> rng_;
    std::vector<MCMCObject*> mcmcObjects, stochastics, deterministics;
    void jump_all(std::vector<MCMCObject*>& v) { for(size_t i = 0; i < v.size(); i++) { v[i]->jump(rng_); } }
    void preserve_all(std::vector<MCMCObject*>& v) { for(size_t i = 0; i < v.size(); i++) { v[i]->preserve(); } }
    void revert_all(std::vector<MCMCObject*>& v) { for(size_t i = 0; i < v.size(); i++) { v[i]->revert(); } }
    void tally_all(std::vector<MCMCObject*>& v) { for(size_t i = 0; i < v.size(); i++) { v[i]->tally(); } }
    void print_all(std::vector<MCMCObject*>& v) { for(size_t i = 0; i < v.size(); i++) { v[i]->print(); } }
    bool bad_logp(const double value) const { return isnan(value) || value == -std::numeric_limits<double>::infinity() ? true : false; }
  public:
    MCModel(): MCModelBase() {}

    void add(MCMCObject& p) {
      mcmcObjects.push_back(&p);
      if(p.isStochastic() && !p.isObserved()) {
        stochastics.push_back(&p);
      }
      if(p.isDeterministc()) {
        deterministics.push_back(&p);
      }
    }

    double acceptance_ratio() const {
      return accepted_ / (accepted_ + rejected_);
    }

    void print() {
      print_all(mcmcObjects);
    }

    bool reject(const double value, const double old_logp) {
      return bad_logp(value) || log(rng_.uniform()) > value - old_logp ? true : false;
    }

    void tune(int iterations, int tuning_step) {
      for(int i = 1; i <= iterations; i++) {
	for(std::vector<MCMCObject*>::iterator it = stochastics.begin(); it != stochastics.end(); it++) {
	  (*it)->component_jump(rng_,*this);
	}
	if(i % tuning_step == 0) {
          std::cout << "tuning at step: " << i << std::endl;
	  for(std::vector<MCMCObject*>::iterator it = stochastics.begin(); it != stochastics.end(); it++) {
	    (*it)->tune();
	  }
	}
      }
    }

    void sample(int iterations, int burn, int thin) {
      tune(burn,50);
      double logp_value,old_logp_value;

      logp_value  = -std::numeric_limits<double>::infinity();
      old_logp_value = -std::numeric_limits<double>::infinity();
      for(int i = 1; i <= iterations; i++) {
        old_logp_value = logp_value;
        preserve_all(mcmcObjects);
        jump_all(stochastics);
        update();
        logp_value = logp();
        //std::cout << "new|old: " << logp_value << "|" << old_logp_value << std::endl;
        if(reject(logp_value, old_logp_value)) {
          revert_all(mcmcObjects);
          logp_value = old_logp_value;
          rejected_ += 1;
        } else {
          accepted_ += 1;
        }
        if(i > burn && (i % thin == 0)) {
          tally_all(mcmcObjects);
        }
      }
    }
  };
} // namespace cppbugs
#endif // MCMC_MODEL_HPP
