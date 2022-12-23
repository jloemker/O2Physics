// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//
// Strangeness reconstruction QA
// =============================
//
// Dedicated task to understand reconstruction
// Special emphasis on PV reconstruction when strangeness is present
// Tested privately, meant to be used on central MC productions now
//
//    Comments, questions, complaints, suggestions?
//    Please write to:
//    david.dobrigkeit.chinellato@cern.ch
//
#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoAHelpers.h"
#include "ReconstructionDataFormats/Track.h"
#include "Common/Core/RecoDecay.h"
#include "Common/Core/trackUtilities.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "PWGLF/DataModel/LFParticleIdentification.h"
#include "Common/Core/TrackSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/PIDResponse.h"
#include "DetectorsBase/Propagator.h"
#include "DetectorsBase/GeometryManager.h"
#include "DataFormatsParameters/GRPObject.h"
#include "DataFormatsParameters/GRPMagField.h"
#include "CCDB/BasicCCDBManager.h"

#include <TFile.h>
#include <TH2F.h>
#include <TProfile.h>
#include <TLorentzVector.h>
#include <Math/Vector4D.h>
#include <TPDGCode.h>
#include <TDatabasePDG.h>
#include <cmath>
#include <array>
#include <cstdlib>
#include "Framework/ASoAHelpers.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using std::array;

namespace o2::aod
{
namespace mccollisionprop
{
DECLARE_SOA_COLUMN(HasRecoCollision, hasRecoCollision, int); //! decay position Z
}
DECLARE_SOA_TABLE(McCollsExtra, "AOD", "MCCOLLSEXTRA",
                  mccollisionprop::HasRecoCollision);
} // namespace o2::aod

// using MyTracks = soa::Join<aod::Tracks, aod::TracksExtra, aod::pidTPCPr>;
using TracksCompleteIU = soa::Join<aod::TracksIU, aod::TracksExtra, aod::TracksCovIU, aod::TracksDCA>;
using TracksCompleteIUMC = soa::Join<aod::TracksIU, aod::TracksExtra, aod::TracksCovIU, aod::TracksDCA, aod::McTrackLabels>;
using V0DataLabeled = soa::Join<aod::V0Datas, aod::McV0Labels>;
using CascMC = soa::Join<aod::CascDataExt, aod::McCascLabels>;
using RecoedMCCollisions = soa::Join<aod::McCollisions, aod::McCollsExtra>;

struct preProcessMCcollisions {
  Produces<aod::McCollsExtra> mcCollsExtra;

  void process(aod::McCollisions const& mccollisions, soa::Join<aod::Collisions, aod::McCollisionLabels> const& collisions)
  {
    for (auto& mccollision : mccollisions) {
      // check if collision successfully reconstructed
      int lExists = 0;
      for (auto& collision : collisions) {
        if (collision.has_mcCollision()) {
          if (mccollision.globalIndex() == collision.mcCollision().globalIndex()) {
            lExists = 1;
          }
        }
      }
      mcCollsExtra(lExists);
    }
  }
};

struct straRecoStudy {
  HistogramRegistry registry{
    "registry",
    {
      // MC generated, pure
      {"hGenK0Short", "hGenK0Short", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenLambda", "hGenLambda", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenAntiLambda", "hGenAntiLambda", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenXiMinus", "hGenXiMinus", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenXiPlus", "hGenXiPlus", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenOmegaMinus", "hGenOmegaMinus", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenOmegaPlus", "hGenOmegaPlus", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},

      // MC generated within reconstructed collisions
      {"hGenWithPVK0Short", "hGenWithPVK0Short", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenWithPVLambda", "hGenWithPVLambda", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenWithPVAntiLambda", "hGenWithPVAntiLambda", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenWithPVXiMinus", "hGenWithPVXiMinus", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenWithPVXiPlus", "hGenWithPVXiPlus", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenWithPVOmegaMinus", "hGenWithPVOmegaMinus", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},
      {"hGenWithPVOmegaPlus", "hGenWithPVOmegaPlus", {HistType::kTH1F, {{100, 0.0f, 10.0f}}}},

      // Invariant mass, reconstructed correctly, versus pT
      {"h2dMassK0Short", "h2dMassK0Short", {HistType::kTH2F, {{100, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {400, 0.400f, 0.600f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h2dMassLambda", "h2dMassLambda", {HistType::kTH2F, {{100, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {400, 1.01f, 1.21f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h2dMassAntiLambda", "h2dMassAntiLambda", {HistType::kTH2F, {{100, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {400, 1.01f, 1.21f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h2dMassXiMinus", "h2dMassXiMinus", {HistType::kTH2F, {{100, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {400, 1.22f, 1.42f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h2dMassXiPlus", "h2dMassXiPlus", {HistType::kTH2F, {{100, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {400, 1.22f, 1.42f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h2dMassOmegaMinus", "h2dMassOmegaMinus", {HistType::kTH2F, {{100, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {400, 1.57f, 1.77f, "Inv. Mass (GeV/c^{2})"}}}},
      {"h2dMassOmegaPlus", "h2dMassOmegaPlus", {HistType::kTH2F, {{100, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {400, 1.57f, 1.77f, "Inv. Mass (GeV/c^{2})"}}}},

      // Very simple QA for each variable
      {"h2dK0ShortQAV0Radius", "h2dK0ShortQAV0Radius", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 50}}}},
      {"h2dK0ShortQADCAV0Dau", "h2dK0ShortQADCAV0Dau", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {100, 0, 2}}}},
      {"h2dK0ShortQADCAPosToPV", "h2dK0ShortQADCAPosToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dK0ShortQADCANegToPV", "h2dK0ShortQADCANegToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dK0ShortQADCAToPV", "h2dK0ShortQADCAToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 2}}}},
      {"h2dK0ShortQAPointingAngle", "h2dK0ShortQAPointingAngle", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 1}}}},

      // Very simple QA for each variable
      {"h2dLambdaQAV0Radius", "h2dLambdaQAV0Radius", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 50}}}},
      {"h2dLambdaQADCAV0Dau", "h2dLambdaQADCAV0Dau", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {100, 0, 2}}}},
      {"h2dLambdaQADCAPosToPV", "h2dLambdaQADCAPosToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dLambdaQADCANegToPV", "h2dLambdaQADCANegToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dLambdaQADCAToPV", "h2dLambdaQADCAToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 2}}}},
      {"h2dLambdaQAPointingAngle", "h2dLambdaQAPointingAngle", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 1}}}},

      // Very simple QA for each variable
      {"h2dXiMinusQAV0Radius", "h2dXiMinusQAV0Radius", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 50}}}},
      {"h2dXiMinusQACascadeRadius", "h2dXiMinusQACascadeRadius", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 50}}}},
      {"h2dXiMinusQADCAV0Dau", "h2dXiMinusQADCAV0Dau", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {100, 0, 2}}}},
      {"h2dXiMinusQADCACascDau", "h2dXiMinusQADCACascDau", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {100, 0, 2}}}},
      {"h2dXiMinusQADCAPosToPV", "h2dXiMinusQADCAPosToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dXiMinusQADCANegToPV", "h2dXiMinusQADCANegToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dXiMinusQADCABachToPV", "h2dXiMinusQADCABachToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dXiMinusQADCACascToPV", "h2dXiMinusQADCACascToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dXiMinusQAPointingAngle", "h2dXiMinusQAPointingAngle", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 1}}}},

      // Very simple QA for each variable
      {"h2dOmegaMinusQAV0Radius", "h2dOmegaMinusQAV0Radius", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 50}}}},
      {"h2dOmegaMinusQACascadeRadius", "h2dOmegaMinusQACascadeRadius", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 50}}}},
      {"h2dOmegaMinusQADCAV0Dau", "h2dOmegaMinusQADCAV0Dau", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {100, 0, 2}}}},
      {"h2dOmegaMinusQADCACascDau", "h2dOmegaMinusQADCACascDau", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {100, 0, 2}}}},
      {"h2dOmegaMinusQADCAPosToPV", "h2dOmegaMinusQADCAPosToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dOmegaMinusQADCANegToPV", "h2dOmegaMinusQADCANegToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dOmegaMinusQADCABachToPV", "h2dOmegaMinusQADCABachToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dOmegaMinusQADCACascToPV", "h2dOmegaMinusQADCACascToPV", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, -2, 2}}}},
      {"h2dOmegaMinusQAPointingAngle", "h2dOmegaMinusQAPointingAngle", {HistType::kTH2F, {{10, 0.0f, 10.0f, "#it{p}_{T} (GeV/c)"}, {200, 0, 1}}}},

      {"hEventSelection", "hEventSelection", {HistType::kTH1F, {{3, -0.5f, 2.5f}}}},
    },
  };

  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  // Selection criteria - compatible with core wagon autodetect
  Configurable<double> v0setting_cospa{"v0setting_cospa", 0.95, "v0setting_cospa"};
  Configurable<float> v0setting_dcav0dau{"v0setting_dcav0dau", 1.0, "v0setting_dcav0dau"};
  Configurable<float> v0setting_dcapostopv{"v0setting_dcapostopv", 0.1, "v0setting_dcapostopv"};
  Configurable<float> v0setting_dcanegtopv{"v0setting_dcanegtopv", 0.1, "v0setting_dcanegtopv"};
  Configurable<float> v0setting_radius{"v0setting_radius", 0.9, "v0setting_radius"};
  Configurable<double> cascadesetting_cospa{"cascadesetting_cospa", 0.95, "cascadesetting_cospa"};
  Configurable<float> cascadesetting_dcacascdau{"cascadesetting_dcacascdau", 1.0, "cascadesetting_dcacascdau"};
  Configurable<float> cascadesetting_dcabachtopv{"cascadesetting_dcabachtopv", 0.1, "cascadesetting_dcabachtopv"};
  Configurable<float> cascadesetting_cascradius{"cascadesetting_cascradius", 0.5, "cascadesetting_cascradius"};
  Configurable<float> cascadesetting_v0masswindow{"cascadesetting_v0masswindow", 0.01, "cascadesetting_v0masswindow"};
  Configurable<float> cascadesetting_mindcav0topv{"cascadesetting_mindcav0topv", 0.01, "cascadesetting_mindcav0topv"};
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  Configurable<bool> event_sel8_selection{"event_sel8_selection", true, "event selection count post sel8 cut"};
  Configurable<bool> event_posZ_selection{"event_posZ_selection", true, "event selection count post poZ cut"};
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  Configurable<int> tpcmincrossedrows{"mincrossedrows", 70, "Minimum crossed rows"};
  Configurable<int> itsminclusters{"itsminclusters", 4, "Minimum ITS clusters"};
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*

  enum evselstep { kEvSelAll = 0,
                   kEvSelBool,
                   kEvSelVtxZ,
                   kEvSelAllSteps };

  std::array<long, kEvSelAllSteps> evselstats;

  void resetHistos()
  {
    for (Int_t ii = 0; ii < kEvSelAllSteps; ii++)
      evselstats[ii] = 0;
  }

  void fillHistos()
  {
    for (Int_t ii = 0; ii < kEvSelAllSteps; ii++)
      registry.fill(HIST("hEventSelection"), ii, evselstats[ii]);
  }

  Filter preFilterMcCollisions = aod::mccollisionprop::hasRecoCollision > 0;

  Filter preFilterCascade =
    nabs(aod::cascdata::dcapostopv) > v0setting_dcapostopv&& nabs(aod::cascdata::dcanegtopv) > v0setting_dcanegtopv&& nabs(aod::cascdata::dcabachtopv) > cascadesetting_dcabachtopv&& aod::cascdata::dcaV0daughters < v0setting_dcav0dau&& aod::cascdata::dcacascdaughters<cascadesetting_dcacascdau && aod::mccasclabel::mcParticleId> - 1;

  Filter preFilterV0 =
    aod::mcv0label::mcParticleId > -1 && nabs(aod::v0data::dcapostopv) > v0setting_dcapostopv&& nabs(aod::v0data::dcanegtopv) > v0setting_dcanegtopv&& aod::v0data::dcaV0daughters < v0setting_dcav0dau;

  void init(InitContext const&)
  {
    resetHistos();

    registry.get<TH1>(HIST("hEventSelection"))->GetXaxis()->SetBinLabel(1, "All collisions");
    registry.get<TH1>(HIST("hEventSelection"))->GetXaxis()->SetBinLabel(2, "Sel8 cut");
    registry.get<TH1>(HIST("hEventSelection"))->GetXaxis()->SetBinLabel(3, "posZ cut");
  }

  void processMC(soa::Join<aod::Collisions, aod::EvSels>::iterator const& collision, soa::Filtered<V0DataLabeled> const& fullV0s, soa::Filtered<CascMC> const& Cascades, TracksCompleteIUMC const& tracks, aod::McParticles const&, aod::V0sLinked const&)
  {
    evselstats[kEvSelAll]++;
    if (event_sel8_selection && !collision.sel8()) {
      return;
    }
    evselstats[kEvSelBool]++;
    if (event_posZ_selection && abs(collision.posZ()) > 10.f) { // 10cm
      return;
    }
    evselstats[kEvSelVtxZ]++;
    for (auto& v0 : fullV0s) {
      // MC association
      auto posPartTrack = v0.posTrack_as<TracksCompleteIUMC>();
      auto negPartTrack = v0.negTrack_as<TracksCompleteIUMC>();
      if (!v0.has_mcParticle() || !posPartTrack.has_mcParticle() || !negPartTrack.has_mcParticle())
        continue;
      auto v0mc = v0.mcParticle();
      if (TMath::Abs(v0mc.y()) > 0.5)
        continue;

      if (posPartTrack.itsNCls() < itsminclusters || negPartTrack.itsNCls() < itsminclusters)
        continue;
      if (posPartTrack.tpcNClsCrossedRows() < tpcmincrossedrows || negPartTrack.tpcNClsCrossedRows() < tpcmincrossedrows)
        continue;

      if (v0mc.pdgCode() == 310) {
        registry.fill(HIST("h2dK0ShortQAV0Radius"), v0.pt(), v0.v0radius());
        registry.fill(HIST("h2dK0ShortQADCAV0Dau"), v0.pt(), v0.dcaV0daughters());
        registry.fill(HIST("h2dK0ShortQADCAPosToPV"), v0.pt(), v0.posTrack_as<TracksCompleteIUMC>().dcaXY());
        registry.fill(HIST("h2dK0ShortQADCANegToPV"), v0.pt(), v0.negTrack_as<TracksCompleteIUMC>().dcaXY());
        registry.fill(HIST("h2dK0ShortQADCAToPV"), v0.pt(), v0.dcav0topv(collision.posX(), collision.posY(), collision.posZ()));
        registry.fill(HIST("h2dK0ShortQAPointingAngle"), v0.pt(), TMath::ACos(v0.v0cosPA(collision.posX(), collision.posY(), collision.posZ())));
      }
      if (v0mc.pdgCode() == 3122) {
        registry.fill(HIST("h2dLambdaQAV0Radius"), v0.pt(), v0.v0radius());
        registry.fill(HIST("h2dLambdaQADCAV0Dau"), v0.pt(), v0.dcaV0daughters());
        registry.fill(HIST("h2dLambdaQADCAPosToPV"), v0.pt(), v0.posTrack_as<TracksCompleteIUMC>().dcaXY());
        registry.fill(HIST("h2dLambdaQADCANegToPV"), v0.pt(), v0.negTrack_as<TracksCompleteIUMC>().dcaXY());
        registry.fill(HIST("h2dLambdaQADCAToPV"), v0.pt(), v0.dcav0topv(collision.posX(), collision.posY(), collision.posZ()));
        registry.fill(HIST("h2dLambdaQAPointingAngle"), v0.pt(), TMath::ACos(v0.v0cosPA(collision.posX(), collision.posY(), collision.posZ())));
      }

      if (v0.v0radius() > v0setting_radius) {
        if (v0.v0cosPA(collision.posX(), collision.posY(), collision.posZ()) > v0setting_cospa) {
          if (v0.dcaV0daughters() < v0setting_dcav0dau) {
            //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
            // Fill invariant masses
            if (v0mc.pdgCode() == 310)
              registry.fill(HIST("h2dMassK0Short"), v0.pt(), v0.mK0Short());
            if (v0mc.pdgCode() == 3122)
              registry.fill(HIST("h2dMassLambda"), v0.pt(), v0.mLambda());
            if (v0mc.pdgCode() == -3122)
              registry.fill(HIST("h2dMassAntiLambda"), v0.pt(), v0.mAntiLambda());
            //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
          }
        }
      }
    } // end v0 loop

    for (auto& casc : Cascades) {
      // MC association
      if (!casc.has_mcParticle())
        return;
      auto cascmc = casc.mcParticle();
      if (TMath::Abs(cascmc.y()) > 0.5)
        continue;

      auto bachPartTrack = casc.bachelor_as<TracksCompleteIUMC>();

      auto v0index = casc.v0_as<o2::aod::V0sLinked>();
      if (!(v0index.has_v0Data())) {
        continue;
      }
      auto v0 = v0index.v0Data_as<V0DataLabeled>(); // de-reference index to correct v0data in case it exists
      auto posPartTrack = v0.posTrack_as<TracksCompleteIUMC>();
      auto negPartTrack = v0.negTrack_as<TracksCompleteIUMC>();

      if (posPartTrack.itsNCls() < itsminclusters || negPartTrack.itsNCls() < itsminclusters || bachPartTrack.itsNCls() < itsminclusters)
        continue;
      if (posPartTrack.tpcNClsCrossedRows() < tpcmincrossedrows || negPartTrack.tpcNClsCrossedRows() < tpcmincrossedrows || bachPartTrack.itsNCls() < tpcmincrossedrows)
        continue;

      if (cascmc.pdgCode() == 3312) {
        registry.fill(HIST("h2dXiMinusQAV0Radius"), casc.pt(), casc.v0radius());
        registry.fill(HIST("h2dXiMinusQACascadeRadius"), casc.pt(), casc.cascradius());
        registry.fill(HIST("h2dXiMinusQADCAV0Dau"), casc.pt(), casc.dcaV0daughters());
        registry.fill(HIST("h2dXiMinusQADCACascDau"), casc.pt(), casc.dcacascdaughters());
        registry.fill(HIST("h2dXiMinusQADCAPosToPV"), casc.pt(), casc.dcapostopv());
        registry.fill(HIST("h2dXiMinusQADCANegToPV"), casc.pt(), casc.dcanegtopv());
        registry.fill(HIST("h2dXiMinusQADCABachToPV"), casc.pt(), casc.dcabachtopv());
        registry.fill(HIST("h2dXiMinusQADCACascToPV"), casc.pt(), casc.dcacasctopv());
        registry.fill(HIST("h2dXiMinusQAPointingAngle"), casc.pt(), TMath::ACos(casc.casccosPA(collision.posX(), collision.posY(), collision.posZ())));
      }
      if (cascmc.pdgCode() == 3334) {
        registry.fill(HIST("h2dOmegaMinusQAV0Radius"), casc.pt(), casc.v0radius());
        registry.fill(HIST("h2dOmegaMinusQACascadeRadius"), casc.pt(), casc.cascradius());
        registry.fill(HIST("h2dOmegaMinusQADCAV0Dau"), casc.pt(), casc.dcaV0daughters());
        registry.fill(HIST("h2dOmegaMinusQADCACascDau"), casc.pt(), casc.dcacascdaughters());
        registry.fill(HIST("h2dOmegaMinusQADCAPosToPV"), casc.pt(), casc.dcapostopv());
        registry.fill(HIST("h2dOmegaMinusQADCANegToPV"), casc.pt(), casc.dcanegtopv());
        registry.fill(HIST("h2dOmegaMinusQADCABachToPV"), casc.pt(), casc.dcabachtopv());
        registry.fill(HIST("h2dOmegaMinusQADCACascToPV"), casc.pt(), casc.dcacasctopv());
        registry.fill(HIST("h2dOmegaMinusQAPointingAngle"), casc.pt(), TMath::ACos(casc.casccosPA(collision.posX(), collision.posY(), collision.posZ())));
      }

      if (casc.v0radius() > v0setting_radius && casc.cascradius() > cascadesetting_cascradius) {
        if (casc.v0cosPA(collision.posX(), collision.posY(), collision.posZ()) > v0setting_cospa) {
          if (casc.casccosPA(collision.posX(), collision.posY(), collision.posZ()) > cascadesetting_cospa) {
            if (casc.dcaV0daughters() < v0setting_dcav0dau) {
              //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
              // Fill invariant masses
              if (cascmc.pdgCode() == 3312)
                registry.fill(HIST("h2dMassXiMinus"), casc.pt(), casc.mXi());
              if (cascmc.pdgCode() == -3312)
                registry.fill(HIST("h2dMassXiPlus"), casc.pt(), casc.mXi());
              if (cascmc.pdgCode() == 3334)
                registry.fill(HIST("h2dMassOmegaMinus"), casc.pt(), casc.mOmega());
              if (cascmc.pdgCode() == -3334)
                registry.fill(HIST("h2dMassOmegaPlus"), casc.pt(), casc.mOmega());
              //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
            }
          }
        }
      }
    }
    fillHistos();
    resetHistos();
  }
  PROCESS_SWITCH(straRecoStudy, processMC, "Regular analysis", false);

  void processGeneratedReconstructible(soa::Filtered<RecoedMCCollisions>::iterator const& collision, aod::McParticles const& mcParticles)
  {
    // check if collision successfully reconstructed
    for (auto& mcp : mcParticles) {
      if (TMath::Abs(mcp.y()) < 0.5) {
        if (mcp.pdgCode() == 310)
          registry.fill(HIST("hGenWithPVK0Short"), mcp.pt());
        if (mcp.pdgCode() == 3122)
          registry.fill(HIST("hGenWithPVLambda"), mcp.pt());
        if (mcp.pdgCode() == -3122)
          registry.fill(HIST("hGenWithPVAntiLambda"), mcp.pt());
        if (mcp.pdgCode() == 3312)
          registry.fill(HIST("hGenWithPVXiMinus"), mcp.pt());
        if (mcp.pdgCode() == -3312)
          registry.fill(HIST("hGenWithPVXiPlus"), mcp.pt());
        if (mcp.pdgCode() == 3334)
          registry.fill(HIST("hGenWithPVOmegaMinus"), mcp.pt());
        if (mcp.pdgCode() == -3334)
          registry.fill(HIST("hGenWithPVOmegaPlus"), mcp.pt());
      }
    }
  }
  PROCESS_SWITCH(straRecoStudy, processGeneratedReconstructible, "generated analysis in events with PV", true);

  void processPureGenerated(aod::McParticles const& mcParticles)
  {
    // check if collision successfully reconstructed
    for (auto& mcp : mcParticles) {
      if (TMath::Abs(mcp.y()) < 0.5) {
        if (mcp.pdgCode() == 310)
          registry.fill(HIST("hGenK0Short"), mcp.pt());
        if (mcp.pdgCode() == 3122)
          registry.fill(HIST("hGenLambda"), mcp.pt());
        if (mcp.pdgCode() == -3122)
          registry.fill(HIST("hGenAntiLambda"), mcp.pt());
        if (mcp.pdgCode() == 3312)
          registry.fill(HIST("hGenXiMinus"), mcp.pt());
        if (mcp.pdgCode() == -3312)
          registry.fill(HIST("hGenXiPlus"), mcp.pt());
        if (mcp.pdgCode() == 3334)
          registry.fill(HIST("hGenOmegaMinus"), mcp.pt());
        if (mcp.pdgCode() == -3334)
          registry.fill(HIST("hGenOmegaPlus"), mcp.pt());
      }
    }
  }
  PROCESS_SWITCH(straRecoStudy, processPureGenerated, "generated analysis in events with PV", true);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<preProcessMCcollisions>(cfgc),
    adaptAnalysisTask<straRecoStudy>(cfgc)};
}
