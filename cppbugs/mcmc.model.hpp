///////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 Whit Armstrong                                     //
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
#include <map>
#include <exception>
#include <boost/random.hpp>
#include <cppbugs/mcmc.rng.hpp>
#include <cppbugs/mcmc.model.base.hpp>
#include <cppbugs/mcmc.object.hpp>
#include <cppbugs/cppbugs.hpp>

namespace cppbugs {
  typedef std::map<void*,MCMCObject*> vmc_map;
  typedef std::map<void*,MCMCObject*>::iterator vmc_map_iter;

  class MCModel {
  private:
    double accepted_;
    double rejected_;
    SpecializedRng<boost::minstd_rand> rng_;
    std::vector<MCMCObject*> mcmcObjects, jumping_stochastics, deterministics;
    std::vector<std::function<double ()> > logp_functors;
    std::function<void ()> update;
    vmc_map data_node_map;

    void jump() { for(auto v : jumping_stochastics) { v->jump(rng_); } }
    void preserve() { for(auto v : mcmcObjects) { v->preserve(); } }
    void revert() { for(auto v : mcmcObjects) { v->revert(); } }
    void set_scale(const double scale) { for(auto v : jumping_stochastics) { v->setScale(scale); } }
    void tally() { for(auto v : mcmcObjects) { v->tally(); } }
    bool bad_logp(const double value) const { return std::isnan(value) || value == -std::numeric_limits<double>::infinity() ? true : false; }
  public:
    MCModel(std::function<void ()> update_): accepted_(0), rejected_(0), update(update_) {}
    ~MCModel() {
      // use data_node_map as delete list
      // only objects allocated by this class are inserted thre
      // addNode allows user allocated objects to enter the mcmcObjects vector
      for(auto m : data_node_map) {
        delete m.second;
      }
    }

    // allows node to be added without being put on the delete list
    // for those who want full control of their memory...
    void addNode(MCMCObject* node) {
      mcmcObjects.push_back(node);
    }

    void initChain() {
      logp_functors.clear();
      jumping_stochastics.clear();
      deterministics.clear();

      for(auto node : mcmcObjects) {
        if(node->isStochastic()) {
          logp_functors.push_back(node->getLikelihoodFunctor());
        }

        if(node->isStochastic() && !node->isObserved()) {
          jumping_stochastics.push_back(node);
        }

        if(node->isDeterministc()) {
          deterministics.push_back(node);
        }
      }
      // init values
      update();
    }

    double calcDimension() {
      double ans(0);

      for(auto v : jumping_stochastics) {
        ans += v->getSize();
      }
      return ans;
    }

    double acceptance_ratio() const {
      return accepted_ / (accepted_ + rejected_);
    }

    void print() {
      for(auto v : deterministics)
        v->print();
    }

    // bool reject(const double value, const double old_logp) {
    //   double r = exp(value - old_logp);
    //   return bad_logp(value) || (rng_.uniform() > r && r < 1)  ? true : false;
    // }

    bool reject(const double value, const double old_logp) {
      return bad_logp(value) || log(rng_.uniform()) > (value - old_logp) ? true : false;
    }

    double logp() const {
      double ans(0);
      for(auto f : logp_functors) {
        ans += f();
      }
      return ans;
    }

    void tune(int iterations, int tuning_step) {
      double logp_value,old_logp_value;
      logp_value  = -std::numeric_limits<double>::infinity();
      old_logp_value = -std::numeric_limits<double>::infinity();

      for(int i = 1; i <= iterations; i++) {
	for(auto it : jumping_stochastics) {
          old_logp_value = logp_value;
          it->preserve();
          it->jump(rng_);
          update();
          logp_value = logp();
          if(reject(logp_value, old_logp_value)) {
            it->revert();
            logp_value = old_logp_value;
            it->reject();
          } else {
            it->accept();
          }
	}
	if(i % tuning_step == 0) {
          //std::cout << "tuning at step: " << i << std::endl;
	  for(auto it : jumping_stochastics) {
	    it->tune();
	  }
	}
      }
    }

    void run(int iterations, int burn, int thin) {
      double logp_value,old_logp_value;
      logp_value  = -std::numeric_limits<double>::infinity();
      old_logp_value = -std::numeric_limits<double>::infinity();
      for(int i = 1; i <= (iterations + burn); i++) {
        old_logp_value = logp_value;
        preserve();
        jump();
        update();
        logp_value = logp();
        if(reject(logp_value, old_logp_value)) {
          revert();
          logp_value = old_logp_value;
          rejected_ += 1;
        } else {
          accepted_ += 1;
        }
        if(i > burn && (i % thin == 0)) {
          tally();
        }
      }

    }

    void sample(int iterations, int burn, int adapt, int thin) {
      //const double scale_num = 2.38;

      if(iterations % thin) {
        std::cout << "ERROR: interations not a multiple of thin." << std::endl;
        return;
      }

      //double d = calcDimension();
      //std::cout << "dim size:" << d << std::endl;
      //double ideal_scale = sqrt(scale_num / pow(d,2));
      //double ideal_scale = scale_num / sqrt(d);
      //std::cout << "ideal_scale: " << ideal_scale << std::endl;
      //set_scale(ideal_scale);

      // setup logp's etc.
      initChain();

      // tuning phase
      tune(adapt,static_cast<int>(adapt/100));

      // sampling
      run(iterations, burn, thin);
    }

    template<typename T>
    Normal<T>& normal(const T& x) {
      Normal<T>* node = new Normal<T>(const_cast<T&>(x),true);
      mcmcObjects.push_back(node);
      data_node_map[(void*)(&x)] = node;
      return *node;
    }

    template<typename T>
    Normal<T>& normal(T& x) {
      Normal<T>* node = new Normal<T>(x,false);
      mcmcObjects.push_back(node);
      data_node_map[(void*)(&x)] = node;
      return *node;
    }

    template<typename T>
    Uniform<T>& uniform(T& x, const bool observed = false) {
      Uniform<T>* node = new Uniform<T>(x, observed);
      data_node_map[(void*)(&x)] = node;
      mcmcObjects.push_back(node);
      return *node;
    }

    template<typename T>
    Gamma<T>& gamma(T& x) {
      Gamma<T>* node = new Gamma<T>(x, false);
      data_node_map[(void*)(&x)] = node;
      mcmcObjects.push_back(node);
      return *node;
    }

    template<typename T>
    Binomial<T>& binomial(const T& x) {
      Binomial<T>* node = new Binomial<T>(const_cast<T&>(x),true);
      mcmcObjects.push_back(node);
      data_node_map[(void*)(&x)] = node;
      return *node;
    }

    template<typename T>
    Deterministic<T>& deterministic(T& x) {
      Deterministic<T>* node = new Deterministic<T>(x);
      data_node_map[(void*)(&x)] = node;
      mcmcObjects.push_back(node);
      return *node;
    }

    template<typename T>
    MCMCSpecialized<T>& getNode(const T& x) {
      vmc_map_iter iter = data_node_map.find((void*)(&x));
      if(iter == data_node_map.end()) {
        throw std::logic_error("node not found.");
      }
      MCMCSpecialized<T>* ans = dynamic_cast<MCMCSpecialized<T>*>(iter->second);
      if(ans == nullptr) {
        throw std::logic_error("invalid node conversion.");
      }
      return *ans;
    }
  };
} // namespace cppbugs
#endif // MCMC_MODEL_HPP
