#ifndef _HGCSimTester_h_
#define _HGCSimTester_h_

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "SimDataFormats/CaloHit/interface/PCaloHitContainer.h"
#include "SimDataFormats/CaloHit/interface/PCaloHit.h"

#include "TTree.h"

/**
   @class HGCSimTester
   @short to test simulated hits
*/


class HGCSimTester : public edm::one::EDAnalyzer<edm::one::SharedResources>
{
  
 public:
  
  explicit HGCSimTester( const edm::ParameterSet& );
  ~HGCSimTester() {}
  virtual void analyze( const edm::Event&, const edm::EventSetup& );
  void endJob();

 private:
 
  const std::array<edm::EDGetTokenT<edm::PCaloHitContainer>, 3> simHitTokens_;
  const double sci_keV2MIP_;

  TTree *tree_;
  std::vector<bool> isSci_;
  std::vector<float> time_, charge_;

};
 

#endif
