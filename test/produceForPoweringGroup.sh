#!/bin/bash

scenarios=(
    3iab_120-600V_200-300-600V
    3iab_120-600V_200-300-800V
    #3iab_120-800V_200-300-800V
    #4iab_120-600V_200-300-600V
    #4iab_120-600V_200-300-800V
    #4iab_120-800V_200-300-800V
)
for s in ${scenarios[@]}; do
   cmsRun test/hgcsiopscan_cfg.py \
	doseMap=SimCalorimetry/HGCalSimProducers/data/doseParams_3000fb_fluka-3.7.20.txt \
	uvmapfile=UserCode/HGCElectronicsValidation/data/geomnew_corrected_360.txt \
	savePadInfo=True scenario=${s} output=SiOpScan_${s}_v5.root; 
done
