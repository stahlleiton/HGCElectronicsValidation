#include "UserCode/HGCElectronicsValidation/interface/HGCDigiTester.h"
#include "DetectorDescription/OfflineDBLoader/interface/GeometryInfoDump.h"
#include "Geometry/Records/interface/IdealGeometryRecord.h"
#include "SimDataFormats/CaloTest/interface/HGCalTestNumbering.h"
#include "SimG4CMS/Calo/interface/CaloHitID.h"
#include "DetectorDescription/Core/interface/DDFilter.h"
#include "DetectorDescription/Core/interface/DDFilteredView.h"
#include "DetectorDescription/Core/interface/DDSolid.h"
#include "DataFormats/GeometryVector/interface/Basic3DVector.h"
#include "CLHEP/Units/GlobalSystemOfUnits.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"
#include "DataFormats/ForwardDetId/interface/HGCSiliconDetIdToROC.h"
#include "FWCore/Utilities/interface/RandomNumberGenerator.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "CLHEP/Geometry/Point3D.h"
#include "CLHEP/Geometry/Plane3D.h"
#include "CLHEP/Geometry/Vector3D.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "fastjet/ClusterSequence.hh"

#include <fstream>
#include <iostream>
#include <cassert>
#include <math.h>
#include <TVector3.h>

using namespace std;
using namespace fastjet;

typedef math::XYZTLorentzVector LorentzVector;
typedef math::XYZPoint Point;

//
// PLUGIN IMPLEMENTATION
//


//
HGCDigiTester::HGCDigiTester( const edm::ParameterSet &iConfig ) 
  : simHitsCEE_( consumes<std::vector<PCaloHit>>(edm::InputTag("g4SimHits", "HGCHitsEE")) ),
    simHitsCEH_( consumes<std::vector<PCaloHit>>(edm::InputTag("g4SimHits", "HGCHitsHEfront")) ),
    simHitsCEHSci_( consumes<std::vector<PCaloHit>>(edm::InputTag("g4SimHits", "HGCHitsHEback")) ),
    digisCEE_( consumes<HGCalDigiCollection>(edm::InputTag("simHGCalUnsuppressedDigis","EE")) ),
    digisCEH_( consumes<HGCalDigiCollection>(edm::InputTag("simHGCalUnsuppressedDigis","HEfront")) ),
    digisCEHSci_( consumes<HGCalDigiCollection>(edm::InputTag("simHGCalUnsuppressedDigis","HEback")) ),
    caloParticlesToken_(consumes<CaloParticleCollection>(edm::InputTag("mix:MergedCaloTruth"))),
    clusters_token_( consumes<std::vector<reco::CaloCluster> >(edm::InputTag("hgcalLayerClusters",""))),
    genParticles_( consumes<std::vector<reco::GenParticle>>(edm::InputTag("genParticles")) ),
    genT0_( consumes<float>(edm::InputTag("genParticles:t0")) ),
    caloGeomToken_(esConsumes<CaloGeometry, CaloGeometryRecord>())
{   
  hardProcOnly_=iConfig.getParameter<bool>("hardProcOnly");
  onlyROCTree_=iConfig.getParameter<bool>("onlyROCTree");

  //configure noise map
  std::string digitizers[]={"hgceeDigitizer","hgcehDigitizer","hgcehsciDigitizer"};
  for(size_t i=0; i<3; i++) {
    edm::ParameterSet cfg(iConfig.getParameter<edm::ParameterSet>(digitizers[i]));
    edm::ParameterSet digiCfg(cfg.getParameter<edm::ParameterSet>("digiCfg"));
    edm::ParameterSet feCfg(digiCfg.getParameter<edm::ParameterSet>("feCfg"));

    //Si specific
    if(i<2) {
      HGCalSiNoiseMap<HGCSiliconDetId> *scal=new HGCalSiNoiseMap<HGCSiliconDetId>;
      scal->setDoseMap(digiCfg.getParameter<edm::ParameterSet>("noise_fC").template getParameter<std::string>("doseMap"),
                             digiCfg.getParameter<edm::ParameterSet>("noise_fC").template getParameter<uint32_t>("scaleByDoseAlgo"));
      scal->setFluenceScaleFactor(digiCfg.getParameter<edm::ParameterSet>("noise_fC").getParameter<double>("scaleByDoseFactor"));
      scal->setIleakParam(digiCfg.getParameter<edm::ParameterSet>("ileakParam").template getParameter<std::vector<double>>("ileakParam"));
      scal->setCceParam(digiCfg.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParamFine"),
                        digiCfg.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParamThin"),
                        digiCfg.getParameter<edm::ParameterSet>("cceParams").template getParameter<std::vector<double>>("cceParamThick"));
      scal_.push_back( scal );

      std::vector<double> avg_mip=iConfig.getParameter<std::vector<double> >(i==0? "hgcee_fCPerMIP" : "hgceh_fCPerMIP");
      for(auto v:avg_mip)avg_mipfC_[i].push_back(v);
    }

    //Sci-specific
    if(i==2) {
      scalSci_ = new HGCalSciNoiseMap;
      scalSci_->setDoseMap(digiCfg.getParameter<edm::ParameterSet>("noise").template getParameter<std::string>("doseMap"),
                           digiCfg.getParameter<edm::ParameterSet>("noise").template getParameter<uint32_t>("scaleByDoseAlgo"));
      scalSci_->setReferenceDarkCurrent(digiCfg.getParameter<edm::ParameterSet>("noise").template getParameter<double>("referenceIdark"));
      scalSci_->setFluenceScaleFactor(digiCfg.getParameter<edm::ParameterSet>("noise").getParameter<double>("scaleByDoseFactor"));
      scalSci_->setSipmMap(digiCfg.getParameter<edm::ParameterSet>("noise").template getParameter<std::string>("sipmMap"));
      sci_keV2MIP_ = iConfig.getParameter<double>("hgcehsci_keV2DIGI");
    }

    bxTime_[i]=cfg.getParameter<double>("bxTime");
    tofDelay_[i]=cfg.getParameter<double>("tofDelay");

    mipTarget_[i]=feCfg.getParameter<uint32_t>("targetMIPvalue_ADC");
    tdcOnset_fC_[i]=feCfg.getParameter<double>("tdcOnset_fC");
    tdcLSB_[i]=feCfg.getParameter<double>("tdcSaturation_fC") / pow(2., feCfg.getParameter<uint32_t>("tdcNbits") );
    vanilla_adcLSB_fC_[i] = feCfg.getParameter<double>("adcSaturation_fC") / pow(2., feCfg.getParameter<uint32_t>("adcNbits") );
    toaLSB_ns_[i] = feCfg.getParameter<double>("toaLSB_ns");
  }
  useTDCOnsetAuto_ = iConfig.getParameter<bool>("useTDCOnsetAuto");
  useVanillaCfg_ = iConfig.getParameter<bool>("useVanillaCfg");

  maxDeltaR_ = iConfig.getParameter<double>("maxDeltaR");
  clustJetAlgo_ =iConfig.getParameter<int>("clustJetAlgo");

  event_=0;
  edm::Service<TFileService> fs;
  if(!onlyROCTree_) {
    tree_ = fs->make<TTree>("hits","hits");
    tree_->Branch("genergy",&genergy_,"genergy/F");
    tree_->Branch("gpt",&gpt_,"gpt/F");
    tree_->Branch("geta",&geta_,"geta/F");
    tree_->Branch("gphi",&gphi_,"gphi/F");
    tree_->Branch("gvradius",&gvradius_,"gvradius/F");
    tree_->Branch("gvz",&gvz_,"gvz/F");
    tree_->Branch("gvt",&gvt_,"gvt/F");
    tree_->Branch("gbeta",&gbeta_,"gbeta/F");
    tree_->Branch("event",&event_,"event/I");
    tree_->Branch("detid",&detid_,"detid/i");
    tree_->Branch("layer",&layer_,"layer/I");
    tree_->Branch("u",&u_,"u/I");  //u or iphi
    tree_->Branch("v",&v_,"v/I");  //v or ieta
    tree_->Branch("roc",&roc_,"roc/I");
    tree_->Branch("adc",&adc_,"adc/I");
    tree_->Branch("toa",&toa_,"toa/I");
    tree_->Branch("toarec",&toarec_,"toarec/F");
    tree_->Branch("toasim",&toasim_,"toasim/F");
    tree_->Branch("gain",&gain_,"gain/I");
    tree_->Branch("qsim",&qsim_,"qsim/F");
    tree_->Branch("qrec",&qrec_,"qrec/F");
    tree_->Branch("mipsim",&mipsim_,"mipsim/F");
    tree_->Branch("miprec",&miprec_,"miprec/F");
    tree_->Branch("avgmiprec",&avgmiprec_,"avgmiprec/F");
    tree_->Branch("avgmipsim",&avgmipsim_,"avgmipsim/F");
    tree_->Branch("cce",&cce_,"cce/F");
    tree_->Branch("eta",&eta_,"eta/F");
    tree_->Branch("radius",&radius_,"radius/F");
    tree_->Branch("z",&z_,"z/F");
    tree_->Branch("isSat",&isSat_,"isSat/I");
    tree_->Branch("isTOT",&isToT_,"isTOT/I");
    tree_->Branch("thick",&thick_,"thick/I");
    tree_->Branch("isSci",&isSci_,"isSci/I");
    tree_->Branch("crossCalo",&crossCalo_,"crossCalo/I");
    tree_->Branch("inShower",&inShower_,"inShower/I");
  }

  rocTree_ = fs->make<TTree>("rocs","rocs");
  rocTree_->Branch("event",&event_,"event/I");
  rocTree_->Branch("layer",&layer_,"layer/I");
  rocTree_->Branch("side",&side_,"side/O");
  rocTree_->Branch("u",&u_,"u/I");
  rocTree_->Branch("v",&v_,"v/I");
  rocTree_->Branch("roc",&roc_,"roc/I");
  rocTree_->Branch("nhits",&nhits_,"nhits/I");
  rocTree_->Branch("miprec",&miprec_,"miprec/F");
}

//
void HGCDigiTester::endJob()
{
}

//
void HGCDigiTester::analyze( const edm::Event &iEvent, const edm::EventSetup &iSetup)
{
  event_++;

  rocDeposits_t rocs;

  const auto& genT0 = iEvent.get(genT0_);
  edm::Handle<std::vector<reco::GenParticle> > genParticlesHandle;
  iEvent.getByToken(genParticles_, genParticlesHandle);
  std::map<int,LorentzVector> photons;
  std::map<int,Point> photonVertex;
  for(size_t i = 0; i < genParticlesHandle->size(); ++i )  {    
    const reco::GenParticle &p = (*genParticlesHandle)[i];
    if (p.status()!=1) continue;
    int idx(p.pz()>0 ? 1 : -1);
    photons[idx]=p.p4();
    photonVertex[idx]=p.vertex();
  }

  //flag if particle interacted before calorimeter
  crossCalo_ = -1;
  edm::Handle<CaloParticleCollection> caloParticlesHandle;
  iEvent.getByToken(caloParticlesToken_, caloParticlesHandle);
  if(caloParticlesHandle.isValid() && caloParticlesHandle->size()==1) {
    auto const& cp = (*caloParticlesHandle)[0];
    crossCalo_ = std::abs(cp.pdgId()) != 11 ? cp.g4Tracks()[0].crossedBoundary() : 1;
  }

  //read the layer clusters and build the pseudoparticles to cluster
  std::set<uint32_t> showerDetIds;
  edm::Handle<std::vector<reco::CaloCluster>> clusterHandle;
  iEvent.getByToken(clusters_token_, clusterHandle);
  if(clusterHandle.isValid()) {
    std::vector<PseudoJet> pseudoParticles;
    for(size_t ic=0; ic<clusterHandle->size(); ic++) {
      const auto &c = clusterHandle->at(ic);
      if(c.algo()!=8) continue; //HGCAL clusters
      //save as pseudo-jet for the clustering
      const auto& en = c.energy();
      const auto evec = TVector3(c.x(),c.y(),c.z()).Unit();
      auto ip = PseudoJet(en*evec.X(), en*evec.Y(), en*evec.Z(), en);
      ip.set_user_index(ic);
      pseudoParticles.push_back( ip );
    }
    //run a jet algorithm
    JetDefinition jet_def( (JetAlgorithm)(clustJetAlgo_), maxDeltaR_);
    ClusterSequence cs(pseudoParticles, jet_def);
    const auto jets = sorted_by_pt(cs.inclusive_jets());
    if(jets.size()>0) {
      for(auto jconst :  jets[0].constituents()) {
        const auto& ic = jconst.user_index();
        const auto& c = clusterHandle->at(ic);
        const auto& hf = c.hitsAndFractions();
        for(auto hfp : hf)
          showerDetIds.insert(hfp.first);
      }
    }
  }

  //read sim hits
  //acumulate total energy deposited in each DetId
  edm::Handle<edm::PCaloHitContainer> simHitsCEE,simHitsCEH,simHitsCEHSci;
  iEvent.getByToken(simHitsCEE_,    simHitsCEE);
  iEvent.getByToken(simHitsCEH_,    simHitsCEH);
  iEvent.getByToken(simHitsCEHSci_, simHitsCEHSci);

  std::map<uint32_t,double> simE;
  for(size_t i=0; i<3; i++) {
    const std::vector<PCaloHit> &simHits((i==0 ? *simHitsCEE : (i==1 ? *simHitsCEH : *simHitsCEHSci)));
    for(auto sh : simHits) {
      //assign a reco-id
      uint32_t key(sh.id());
      if(simE.find(key)==simE.end()) simE[key]=0.;
      simE[key]=simE[key]+sh.energy();
    }
  }    

  //read digis
  edm::Handle<HGCalDigiCollection> digisCEE,digisCEH,digisCEHSci;
  iEvent.getByToken(digisCEE_,digisCEE);
  iEvent.getByToken(digisCEH_,digisCEH);
  iEvent.getByToken(digisCEHSci_,digisCEHSci);

  //read geometry components for HGCAL
  edm::ESHandle<CaloGeometry> geom = iSetup.getHandle(caloGeomToken_);
  const HGCalGeometry *geoCEE = static_cast<const HGCalGeometry*>(geom->getSubdetectorGeometry(DetId::HGCalEE, ForwardSubdetector::ForwardEmpty));
  const HGCalTopology &topoCEE = geoCEE->topology();
  const HGCalDDDConstants &dddConstCEE = topoCEE.dddConstants();
  const HGCalGeometry *geoCEH = static_cast<const HGCalGeometry*>(geom->getSubdetectorGeometry(DetId::HGCalHSi, ForwardSubdetector::ForwardEmpty));
  const HGCalTopology &topoCEH = geoCEH->topology();
  const HGCalDDDConstants &dddConstCEH = topoCEH.dddConstants();
  const HGCalGeometry *geoCEHSci = static_cast<const HGCalGeometry*>(geom->getSubdetectorGeometry(DetId::HGCalHSc, ForwardSubdetector::ForwardEmpty));

  //utility to map detid to ROC
  HGCSiliconDetIdToROC sid2roc;

  //set the geometry
  scal_[0]->setGeometry(geoCEE, HGCalSiNoiseMap<HGCSiliconDetId>::AUTO, mipTarget_[0]);
  scal_[1]->setGeometry(geoCEH, HGCalSiNoiseMap<HGCSiliconDetId>::AUTO, mipTarget_[1]);
  scalSci_->setGeometry(geoCEHSci);

  //loop over digis
  size_t itSample(2);
  for(size_t i=0; i<3; i++) {

    const HGCalDigiCollection &digis(i==0 ? *digisCEE : (i==1 ? *digisCEH : *digisCEHSci) );
    const std::vector<PCaloHit> &simHits(i==0 ? *simHitsCEE : (i==1 ? *simHitsCEH : *simHitsCEHSci) );
    const auto geo = (i==0 ? geoCEE : (i==1 ? geoCEH : geoCEHSci) );

    std::map<uint32_t,double> simToA, simTE;
    constexpr double c_cm_ns = CLHEP::c_light * CLHEP::ns / CLHEP::cm;
    for(auto sh : simHits) {
      uint32_t key(sh.id());
      const auto ene = sh.energy();
      const auto tHGCAL = sh.time();
      const auto dist2center = geo->getPosition(key).mag();
      const auto toa = tHGCAL - dist2center/c_cm_ns + tofDelay_[i];
      if (std::floor(toa / bxTime_[i]) != 0) continue;
      simToA[key] += toa*ene;
      simTE[key] += ene;
    }

    for(auto d : digis) {

      //check if it's matched to a simId
      uint32_t key( d.id().rawId() );
      detid_=key;
      bool simEexists(true); 
      if(simE.find(key)==simE.end()){
        if(hardProcOnly_) continue;
        simEexists=false;
      }
      //read digi (in-time sample only)
      uint32_t adc(d.sample(itSample).data() );
      adc_=adc;
      toa_=d.sample(itSample).getToAValid() ? d.sample(itSample).toa() : 0;
      toarec_=d.sample(itSample).getToAValid() ? ((toa_+0.5)*toaLSB_ns_[i] - tofDelay_[i]) : -999;
      toasim_=simToA.find(key)!=simToA.end() ? (simToA[key]/simTE[key] - tofDelay_[i]) : -999;
      isToT_=d.sample(itSample).mode();
      inShower_=!showerDetIds.empty() ? showerDetIds.find(key)!=showerDetIds.end() : -1;

      isSat_=false;
      //if(!isToT_ && adc>1020) isSat_=true; // never happens
      if(isToT_ && adc==4095) isSat_=true;
      
      //reset common variables
      double adcLSB(vanilla_adcLSB_fC_[i]),mipEqfC(1.0),avgMipEqfC(1.0);
      qsim_=0;
      qrec_=0.;
      cce_=1.;
      thick_=-1;
      layer_=0;
      u_=0;
      v_=0;
      roc_=0;
      radius_=0;
      z_=0;
      int zside=0;

      HGCalSiNoiseMap<HGCSiliconDetId>::GainRange_t gain = (HGCalSiNoiseMap<HGCSiliconDetId>::GainRange_t)d.sample(itSample).gain();
      gain_ = gain;

      //scintillator does not yet attribute different gains
      if(!useVanillaCfg_ && i!=2) {
        adcLSB=scal_[0]->getLSBPerGain()[gain];
      }

      //Si-specific
      if(i<2) {

        //simulated charge
        qsim_ = simEexists ? simE[key] * 1.0e6 * 0.044259 : 0.; // GeV -> fC  (1000 eV / 3.62 (eV per e) / 6.24150934e3 (e per fC))

        //get the conditions for this det id
        HGCSiliconDetId cellId(d.id());
        u_ = cellId.waferUV().first;
        v_ = cellId.waferUV().second;
        roc_    = sid2roc.getROCNumber(cellId);
        HGCalSiNoiseMap<HGCSiliconDetId>::SiCellOpCharacteristics siop 
          = scal_[i]->getSiCellOpCharacteristics(cellId);
        cce_       = siop.core.cce;
        thick_     = cellId.type();
        mipEqfC    = scal_[i]->getMipEqfC()[thick_];
        avgMipEqfC = avg_mipfC_[i][thick_];
        isSci_     = false;

        //additional info
        layer_ = cellId.layer();
        
        const HGCalDDDConstants &dddConst(i==0 ? dddConstCEE : dddConstCEH);
        const auto &xy(dddConst.locateCell(cellId.layer(), cellId.waferU(), cellId.waferV(), cellId.cellU(), cellId.cellV(), true, true));
        radius_ = sqrt(std::pow(xy.first, 2) + std::pow(xy.second, 2));  //in cm
        zside=cellId.zside();
        z_ = zside*dddConst.waferZ(layer_,true);

        // get tdcOnset from gain
        if (useTDCOnsetAuto_) {
          tdcOnset_fC_[i] = scal_[i]->getTDCOnsetAuto(gain);
        }
      }

      //Sci-specific
      //maybe here we could also add a ROC ?
      if(i==2) {

        HGCScintillatorDetId scId(d.id());
        u_=scId.iphi();
        v_=scId.ieta();

        //simulated "charge" (in reality this is in MIP units)
        qsim_ = simEexists ? simE[key] *1.0e+6 * sci_keV2MIP_ : 0.; // keV to mip

        //get the conditions for this det id
        //signal scaled by tile and sipm area + dose
        double signal_scale(1.0);
        HGCScintillatorDetId cellId(d.id());
        GlobalPoint global = geoCEHSci->getPosition(cellId);
        radius_ = scalSci_->computeRadius(cellId);
        if(!useVanillaCfg_ && scalSci_->algo()==0) {
          HGCalSciNoiseMap::SiPMonTileCharacteristics sipmOP(scalSci_->scaleByDose(cellId,radius_));
          signal_scale *= sipmOP.lySF;
        }
        cce_ = signal_scale;
      

        mipEqfC    = 1.0;     //the digis are already in MIP units
        avgMipEqfC = 1.0;

        //additional info
        layer_ = cellId.layer();
        z_     = global.z();
        zside  = cellId.zside();
        //zside  = (z_<0 ? -1 : 1);
        thick_ = -1;
        isSci_ = true;
      }
       

      //convert back to charge
      qrec_=(adc+0.5)*adcLSB ;
      if(isToT_) 
        qrec_=tdcOnset_fC_[i]+(adc+0.5)*tdcLSB_[i];          

      //convert charge to #MIPs
      miprec_     = qrec_/mipEqfC;
      mipsim_     = qsim_/mipEqfC;
      avgmiprec_  = qrec_/avgMipEqfC;
      avgmipsim_  = qsim_/avgMipEqfC;

      //additional info
      //eta_    = TMath::ATanH(z_/sqrt(radius_*radius_+z_*z_));
      eta_    = atanh(z_/sqrt(radius_*radius_+z_*z_));

      //for CEH shift by CEE layers
      if(i>0) layer_+=28;

      //MC truth
      genergy_  = photons[zside].energy();
      gpt_      = photons[zside].pt();
      geta_     = photons[zside].eta();
      gphi_     = photons[zside].phi();
      gvradius_ = sqrt(pow(photonVertex[zside].x(),2)+pow(photonVertex[zside].y(),2));
      gvz_      = photonVertex[zside].z();
      gvt_      = genT0;
      gbeta_    = 1./sqrt(1. + pow(photons[zside].mass()/photons[zside].P(), 2));

      //store hit
      if(!onlyROCTree_) tree_->Fill();

      //increment energy for the ROC (Si only)
      if(i==2) continue;
      rocKey_t k(layer_,(z_>0),u_,v_,roc_);
      if(rocs.find(k)==rocs.end()) rocs[k]=rocSummary_t(0.,0.);
      rocs[k].first += 1;
      rocs[k].second += miprec_/cce_;
    }
  }
  
  //save the summary of energy deposits in ROCs
  for(auto r : rocs ){
    layer_=std::get<0>(r.first) ;
    side_=std::get<1>(r.first) ;
    u_=std::get<2>(r.first) ;
    v_=std::get<3>(r.first) ;
    roc_=std::get<4>(r.first) ;
    nhits_=r.second.first;
    miprec_=r.second.second;
    rocTree_->Fill();
  }



}


//define this as a plug-in
DEFINE_FWK_MODULE(HGCDigiTester);
