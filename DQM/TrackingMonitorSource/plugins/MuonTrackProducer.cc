#include "DataFormats/MuonReco/interface/Muon.h"
#include "DataFormats/MuonReco/interface/MuonFwd.h"
#include "DataFormats/MuonReco/interface/MuonSelectors.h"

#include "DataFormats/TrackReco/interface/TrackExtra.h"
#include "DataFormats/TrackReco/interface/TrackExtraFwd.h"
#include "RecoTracker/TrackProducer/interface/TrackProducerBase.h"

#include "DataFormats/TrackReco/interface/HitPattern.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/BeamSpot/interface/BeamSpot.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "TLorentzVector.h"

#include "DQM/TrackingMonitorSource/interface/MuonTrackProducer.h"

using namespace std;
using namespace edm;
//using namespace reco;

MuonTrackProducer::MuonTrackProducer(const edm::ParameterSet& ps) :
  muonTag_(ps.getUntrackedParameter<edm::InputTag>("muonInputTag", edm::InputTag("muons"))),
  bsTag_(ps.getUntrackedParameter<edm::InputTag>("offlineBeamSpot", edm::InputTag("offlineBeamSpot"))),
  muonToken_(consumes<reco::MuonCollection>(muonTag_)),
  bsToken_(consumes<reco::BeamSpot>(bsTag_)),
  maxEta_(ps.getUntrackedParameter<double>("maxEta", 2.1)),
  minPt_(ps.getUntrackedParameter<double>("minPt", 5)),
  maxNormChi2_(ps.getUntrackedParameter<double>("maxNormChi2", 10)),
  maxD0_(ps.getUntrackedParameter<double>("maxD0", 0.02)),
  maxDz_(ps.getUntrackedParameter<double>("maxDz", 20.)),
  minPixelHits_(ps.getUntrackedParameter<uint32_t>("minPixelHits", 1)),
  minStripHits_(ps.getUntrackedParameter<uint32_t>("minStripHits", 8)),
  minChambers_(ps.getUntrackedParameter<uint32_t>("minChambers", 2)),
  minMatches_(ps.getUntrackedParameter<uint32_t>("minMatches", 2)),
  minMatchedStations_(ps.getUntrackedParameter<double>("minMatchedStations", 2)),
  maxIso_(ps.getUntrackedParameter<double>("maxIso", 0.3)),
  minPtHighest_(ps.getUntrackedParameter<double>("minPtHighest", 24)),
  minInvMass_(ps.getUntrackedParameter<double>("minInvMass", 60)),
  maxInvMass_(ps.getUntrackedParameter<double>("maxInvMass", 120))
{
  produces<reco::TrackCollection>("");
}

MuonTrackProducer::~MuonTrackProducer()
{
}

void MuonTrackProducer::produce(edm::Event& iEvent, edm::EventSetup const& iSetup){
  std::unique_ptr< reco::TrackCollection > outputTColl( new reco::TrackCollection() ) ;

  // Read Muon Collection
  edm::Handle<reco::MuonCollection> muonColl;
  iEvent.getByToken(muonToken_, muonColl);

  // and the beamspot
  edm::Handle<reco::BeamSpot> beamSpot;
  iEvent.getByToken(bsToken_, beamSpot);

  std::vector<TLorentzVector> list;
  if (muonColl.isValid()) {
    for (auto const& mu : *muonColl) {
      if (!mu.isGlobalMuon()) continue;
      if (!mu.isPFMuon()) continue;
      if (std::fabs(mu.eta()) >= maxEta_) continue;
      if (mu.pt() < minPt_) continue;

      reco::TrackRef gtk = mu.globalTrack();
      double chi2 = gtk->chi2();
      double ndof = gtk->ndof();
      double chbyndof = (ndof > 0) ? chi2/ndof : 0;
      if (chbyndof >= maxNormChi2_) continue;

      reco::TrackRef tk = mu.innerTrack();
      double trkd0 = tk->d0();
      double trkdz = tk->dz();
      if (beamSpot.isValid()) {
        trkd0 = -(tk->dxy(beamSpot->position()));
        if (std::fabs(trkd0) >= maxD0_) continue;
        trkdz = tk->dz(beamSpot->position()); 
        if (std::fabs(trkdz) >= maxDz_) continue;
      }
      else {
        edm::LogError("MuonTrackProducer") << "Error >> Failed to get BeamSpot for label: "
                                            << bsTag_;
      }
      const reco::HitPattern& hitp = gtk->hitPattern(); 
      if (hitp.numberOfValidPixelHits() < minPixelHits_) continue;
      if (hitp.numberOfValidStripHits() < minStripHits_) continue;
      // Hits/section in the muon chamber
      if (mu.numberOfChambers() < minChambers_) continue;
      if (mu.numberOfMatches() < minMatches_) continue;
      if (mu.numberOfMatchedStations() < minMatchedStations_) continue;
      if (!muon::isGoodMuon(mu, muon::GlobalMuonPromptTight)) continue;

      // PF Isolation      
      const reco::MuonPFIsolation& pfIso04 = mu.pfIsolationR04();
      double absiso = pfIso04.sumChargedHadronPt + std::max(0.0, pfIso04.sumNeutralHadronEt + pfIso04.sumPhotonEt - 0.5 * pfIso04.sumPUPt);
      if (absiso/mu.pt() > maxIso_) continue;

      TLorentzVector lv;
      lv.SetPtEtaPhiE(mu.pt(), mu.eta(), mu.phi(), mu.energy());
      list.push_back(lv); 

      outputTColl->push_back(*tk);
    }
  }
  else {
    edm::LogError("MuonTrackProducer") << "Error >> Failed to get MuonCollection for label: " << muonTag_;
  }

  iEvent.put(std::move(outputTColl));
}

// Define this as a plug-in
#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(MuonTrackProducer);
