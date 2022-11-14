#include "UserCode/HGCElectronicsValidation/interface/HGCSimTester.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

//
// PLUGIN IMPLEMENTATION
//


//
HGCSimTester::HGCSimTester( const edm::ParameterSet &iConfig ) 
  : simHitTokens_{{
      consumes<edm::PCaloHitContainer>(edm::InputTag("g4SimHits", "HGCHitsEE")),
      consumes<edm::PCaloHitContainer>(edm::InputTag("g4SimHits", "HGCHitsHEfront")),
      consumes<edm::PCaloHitContainer>(edm::InputTag("g4SimHits", "HGCHitsHEback"))
    }},
    sci_keV2MIP_(iConfig.getParameter<double>("hgcehsci_keV2DIGI"))
{
  edm::Service<TFileService> fs;
  tree_ = fs->make<TTree>("hits","hits");
  tree_->Branch("isSci",&isSci_);
  tree_->Branch("time",&time_);
  tree_->Branch("charge",&charge_);
}

//
void HGCSimTester::endJob()
{
}

//
void HGCSimTester::analyze( const edm::Event &iEvent, const edm::EventSetup &iSetup)
{
  std::map<uint32_t, std::array<std::vector<const PCaloHit*>, 3> > simHits;
  for (size_t i=0; i<3; i++)
    for (const auto& sh : iEvent.get(simHitTokens_[i]))
      simHits[sh.id()][i].push_back(&sh);

  for (const auto& id : simHits) {
    isSci_.clear();
    time_.clear();
    charge_.clear();
    for (size_t i=0; i<3; i++) {
      for (const auto& sh : id.second[i]) {
        isSci_.push_back(i==2);
        time_.push_back(sh->time());
        charge_.push_back(sh->energy() * 1.0E6 * (i==2 ? sci_keV2MIP_ : 0.044259));
      }
    }
    tree_->Fill();
  }
}


//define this as a plug-in
DEFINE_FWK_MODULE(HGCSimTester);
