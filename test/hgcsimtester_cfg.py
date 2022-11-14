import FWCore.ParameterSet.Config as cms

from Configuration.Eras.Era_Phase2C11I13M9_cff import Phase2C11I13M9
process = cms.Process("ANALYSIS", Phase2C11I13M9)


from FWCore.ParameterSet.VarParsing import VarParsing
options = VarParsing ('standard')
options.register('input',
                 '/eos/cms/store/cmst3/group/hgcal/CMG_studies/Production/FlatEGunK0L_12_3_0_pre5_D86_noPU',
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "input directory")

options.parseArguments()


#set geometry/global tag
process.load("Configuration.StandardSequences.FrontierConditions_GlobalTag_cff")
process.load('Configuration.Geometry.GeometryExtended2026D86Reco_cff')
process.load('Configuration.Geometry.GeometryExtended2026D86_cff')
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, 'auto:phase2_realistic_T21', '')

process.MessageLogger.cerr.threshold = ''
process.MessageLogger.cerr.FwkReport.reportEvery = 500

#source/number events to process
import os
if os.path.isdir(options.input):
    fList = ['file:'+os.path.join(options.input,f) for f in os.listdir(options.input) if '.root' in f]
else:
    fList = ['file:'+x if not x.find('/store')==0 else x for x in options.input.split(',')]
print(fList)


process.source = cms.Source("PoolSource",
                            fileNames = cms.untracked.vstring(fList),
                            duplicateCheckMode = cms.untracked.string("noDuplicateCheck")
                        )
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(options.maxEvents) )

#prepare digitization parameters fo the end of life
process.load('RecoLocalCalo.HGCalRecProducers.HGCalRecHit_cfi')

#analyzer
process.ana = cms.EDAnalyzer("HGCSimTester",
                             hgcehsci_keV2DIGI=process.HGCalRecHit.HGCHEB_keV2DIGI
                         )


process.TFileService = cms.Service("TFileService",
                                   fileName = cms.string(options.output)
                               )


process.p = cms.Path(process.ana)
