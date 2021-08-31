set(FDKAAC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libraries/3rdparty/fdk-aac)

include(CheckFunctionExists)
include(CheckLibraryExists)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CPack)

set(AACDEC_SRC
    ${FDKAAC_DIR}/libAACdec/src/FDK_delay.cpp
    ${FDKAAC_DIR}/libAACdec/src/aac_ram.cpp
    ${FDKAAC_DIR}/libAACdec/src/aac_rom.cpp
    ${FDKAAC_DIR}/libAACdec/src/aacdec_drc.cpp
    ${FDKAAC_DIR}/libAACdec/src/aacdec_hcr.cpp
    ${FDKAAC_DIR}/libAACdec/src/aacdec_hcr_bit.cpp
    ${FDKAAC_DIR}/libAACdec/src/aacdec_hcrs.cpp
    ${FDKAAC_DIR}/libAACdec/src/aacdec_pns.cpp
    ${FDKAAC_DIR}/libAACdec/src/aacdec_tns.cpp
    ${FDKAAC_DIR}/libAACdec/src/aacdecoder.cpp
    ${FDKAAC_DIR}/libAACdec/src/aacdecoder_lib.cpp
    ${FDKAAC_DIR}/libAACdec/src/block.cpp
    ${FDKAAC_DIR}/libAACdec/src/channel.cpp
    ${FDKAAC_DIR}/libAACdec/src/channelinfo.cpp
    ${FDKAAC_DIR}/libAACdec/src/conceal.cpp
    ${FDKAAC_DIR}/libAACdec/src/ldfiltbank.cpp
    ${FDKAAC_DIR}/libAACdec/src/pulsedata.cpp
    ${FDKAAC_DIR}/libAACdec/src/rvlc.cpp
    ${FDKAAC_DIR}/libAACdec/src/rvlcbit.cpp
    ${FDKAAC_DIR}/libAACdec/src/rvlcconceal.cpp
    ${FDKAAC_DIR}/libAACdec/src/stereo.cpp
    ${FDKAAC_DIR}/libAACdec/src/usacdec_ace_d4t64.cpp
    ${FDKAAC_DIR}/libAACdec/src/usacdec_ace_ltp.cpp
    ${FDKAAC_DIR}/libAACdec/src/usacdec_acelp.cpp
    ${FDKAAC_DIR}/libAACdec/src/usacdec_fac.cpp
    ${FDKAAC_DIR}/libAACdec/src/usacdec_lpc.cpp
    ${FDKAAC_DIR}/libAACdec/src/usacdec_lpd.cpp
    ${FDKAAC_DIR}/libAACdec/src/usacdec_rom.cpp
    ${FDKAAC_DIR}/libAACdec/src/aac_ram.h
    ${FDKAAC_DIR}/libAACdec/src/aac_rom.h
    ${FDKAAC_DIR}/libAACdec/src/aacdec_drc.h
    ${FDKAAC_DIR}/libAACdec/src/aacdec_drc_types.h
    ${FDKAAC_DIR}/libAACdec/src/aacdec_hcr.h
    ${FDKAAC_DIR}/libAACdec/src/aacdec_hcr_bit.h
    ${FDKAAC_DIR}/libAACdec/src/aacdec_hcr_types.h
    ${FDKAAC_DIR}/libAACdec/src/aacdec_hcrs.h
    ${FDKAAC_DIR}/libAACdec/src/aacdec_pns.h
    ${FDKAAC_DIR}/libAACdec/src/aacdec_tns.h
    ${FDKAAC_DIR}/libAACdec/src/aacdecoder.h
    ${FDKAAC_DIR}/libAACdec/src/block.h
    ${FDKAAC_DIR}/libAACdec/src/channel.h
    ${FDKAAC_DIR}/libAACdec/src/channelinfo.h
    ${FDKAAC_DIR}/libAACdec/src/conceal.h
    ${FDKAAC_DIR}/libAACdec/src/conceal_types.h
    ${FDKAAC_DIR}/libAACdec/src/FDK_delay.h
    ${FDKAAC_DIR}/libAACdec/src/ldfiltbank.h
    ${FDKAAC_DIR}/libAACdec/src/overlapadd.h
    ${FDKAAC_DIR}/libAACdec/src/pulsedata.h
    ${FDKAAC_DIR}/libAACdec/src/rvlc.h
    ${FDKAAC_DIR}/libAACdec/src/rvlc_info.h
    ${FDKAAC_DIR}/libAACdec/src/rvlcbit.h
    ${FDKAAC_DIR}/libAACdec/src/rvlcconceal.h
    ${FDKAAC_DIR}/libAACdec/src/stereo.h
    ${FDKAAC_DIR}/libAACdec/src/usacdec_ace_d4t64.h
    ${FDKAAC_DIR}/libAACdec/src/usacdec_ace_ltp.h
    ${FDKAAC_DIR}/libAACdec/src/usacdec_acelp.h
    ${FDKAAC_DIR}/libAACdec/src/usacdec_const.h
    ${FDKAAC_DIR}/libAACdec/src/usacdec_fac.h
    ${FDKAAC_DIR}/libAACdec/src/usacdec_lpc.h
    ${FDKAAC_DIR}/libAACdec/src/usacdec_lpd.h
    ${FDKAAC_DIR}/libAACdec/src/usacdec_rom.h)

set(AACENC_SRC
    ${FDKAAC_DIR}/libAACenc/src/aacEnc_ram.cpp
    ${FDKAAC_DIR}/libAACenc/src/aacEnc_rom.cpp
    ${FDKAAC_DIR}/libAACenc/src/aacenc.cpp
    ${FDKAAC_DIR}/libAACenc/src/aacenc_lib.cpp
    ${FDKAAC_DIR}/libAACenc/src/aacenc_pns.cpp
    ${FDKAAC_DIR}/libAACenc/src/aacenc_tns.cpp
    ${FDKAAC_DIR}/libAACenc/src/adj_thr.cpp
    ${FDKAAC_DIR}/libAACenc/src/band_nrg.cpp
    ${FDKAAC_DIR}/libAACenc/src/bandwidth.cpp
    ${FDKAAC_DIR}/libAACenc/src/bit_cnt.cpp
    ${FDKAAC_DIR}/libAACenc/src/bitenc.cpp
    ${FDKAAC_DIR}/libAACenc/src/block_switch.cpp
    ${FDKAAC_DIR}/libAACenc/src/channel_map.cpp
    ${FDKAAC_DIR}/libAACenc/src/chaosmeasure.cpp
    ${FDKAAC_DIR}/libAACenc/src/dyn_bits.cpp
    ${FDKAAC_DIR}/libAACenc/src/grp_data.cpp
    ${FDKAAC_DIR}/libAACenc/src/intensity.cpp
    ${FDKAAC_DIR}/libAACenc/src/line_pe.cpp
    ${FDKAAC_DIR}/libAACenc/src/metadata_compressor.cpp
    ${FDKAAC_DIR}/libAACenc/src/metadata_main.cpp
    ${FDKAAC_DIR}/libAACenc/src/mps_main.cpp
    ${FDKAAC_DIR}/libAACenc/src/ms_stereo.cpp
    ${FDKAAC_DIR}/libAACenc/src/noisedet.cpp
    ${FDKAAC_DIR}/libAACenc/src/pnsparam.cpp
    ${FDKAAC_DIR}/libAACenc/src/pre_echo_control.cpp
    ${FDKAAC_DIR}/libAACenc/src/psy_configuration.cpp
    ${FDKAAC_DIR}/libAACenc/src/psy_main.cpp
    ${FDKAAC_DIR}/libAACenc/src/qc_main.cpp
    ${FDKAAC_DIR}/libAACenc/src/quantize.cpp
    ${FDKAAC_DIR}/libAACenc/src/sf_estim.cpp
    ${FDKAAC_DIR}/libAACenc/src/spreading.cpp
    ${FDKAAC_DIR}/libAACenc/src/tonality.cpp
    ${FDKAAC_DIR}/libAACenc/src/transform.cpp
    ${FDKAAC_DIR}/libAACenc/src/aacenc.h
    ${FDKAAC_DIR}/libAACenc/src/aacenc_pns.h
    ${FDKAAC_DIR}/libAACenc/src/aacEnc_ram.h
    ${FDKAAC_DIR}/libAACenc/src/aacEnc_rom.h
    ${FDKAAC_DIR}/libAACenc/src/aacenc_tns.h
    ${FDKAAC_DIR}/libAACenc/src/adj_thr.h
    ${FDKAAC_DIR}/libAACenc/src/adj_thr_data.h
    ${FDKAAC_DIR}/libAACenc/src/band_nrg.h
    ${FDKAAC_DIR}/libAACenc/src/bandwidth.h
    ${FDKAAC_DIR}/libAACenc/src/bit_cnt.h
    ${FDKAAC_DIR}/libAACenc/src/bitenc.h
    ${FDKAAC_DIR}/libAACenc/src/block_switch.h
    ${FDKAAC_DIR}/libAACenc/src/channel_map.h
    ${FDKAAC_DIR}/libAACenc/src/chaosmeasure.h
    ${FDKAAC_DIR}/libAACenc/src/dyn_bits.h
    ${FDKAAC_DIR}/libAACenc/src/grp_data.h
    ${FDKAAC_DIR}/libAACenc/src/intensity.h
    ${FDKAAC_DIR}/libAACenc/src/interface.h
    ${FDKAAC_DIR}/libAACenc/src/line_pe.h
    ${FDKAAC_DIR}/libAACenc/src/metadata_compressor.h
    ${FDKAAC_DIR}/libAACenc/src/metadata_main.h
    ${FDKAAC_DIR}/libAACenc/src/mps_main.h
    ${FDKAAC_DIR}/libAACenc/src/ms_stereo.h
    ${FDKAAC_DIR}/libAACenc/src/noisedet.h
    ${FDKAAC_DIR}/libAACenc/src/pns_func.h
    ${FDKAAC_DIR}/libAACenc/src/pnsparam.h
    ${FDKAAC_DIR}/libAACenc/src/pre_echo_control.h
    ${FDKAAC_DIR}/libAACenc/src/psy_configuration.h
    ${FDKAAC_DIR}/libAACenc/src/psy_const.h
    ${FDKAAC_DIR}/libAACenc/src/psy_data.h
    ${FDKAAC_DIR}/libAACenc/src/psy_main.h
    ${FDKAAC_DIR}/libAACenc/src/qc_data.h
    ${FDKAAC_DIR}/libAACenc/src/qc_main.h
    ${FDKAAC_DIR}/libAACenc/src/quantize.h
    ${FDKAAC_DIR}/libAACenc/src/sf_estim.h
    ${FDKAAC_DIR}/libAACenc/src/spreading.h
    ${FDKAAC_DIR}/libAACenc/src/tns_func.h
    ${FDKAAC_DIR}/libAACenc/src/tonality.h
    ${FDKAAC_DIR}/libAACenc/src/transform.h)

set(ARITHCODING_SRC
    ${FDKAAC_DIR}/libArithCoding/src/ac_arith_coder.cpp)

set(DRCDEC_SRC
    ${FDKAAC_DIR}/libDRCdec/src/FDK_drcDecLib.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_gainDecoder.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_reader.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_rom.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_selectionProcess.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_tools.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcGainDec_init.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcGainDec_preprocess.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcGainDec_process.cpp
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_gainDecoder.h
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_reader.h
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_rom.h
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_selectionProcess.h
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_tools.h
    ${FDKAAC_DIR}/libDRCdec/src/drcDec_types.h
    ${FDKAAC_DIR}/libDRCdec/src/drcDecoder.h
    ${FDKAAC_DIR}/libDRCdec/src/drcGainDec_init.h
    ${FDKAAC_DIR}/libDRCdec/src/drcGainDec_preprocess.h
    ${FDKAAC_DIR}/libDRCdec/src/drcGainDec_process.h)

set(FDK_SRC
    ${FDKAAC_DIR}/libFDK/src/FDK_bitbuffer.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_core.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_crc.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_decorrelate.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_hybrid.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_lpc.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_matrixCalloc.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_qmf_domain.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_tools_rom.cpp
    ${FDKAAC_DIR}/libFDK/src/FDK_trigFcts.cpp
    ${FDKAAC_DIR}/libFDK/src/autocorr2nd.cpp
    ${FDKAAC_DIR}/libFDK/src/dct.cpp
    ${FDKAAC_DIR}/libFDK/src/fft.cpp
    ${FDKAAC_DIR}/libFDK/src/fft_rad2.cpp
    ${FDKAAC_DIR}/libFDK/src/fixpoint_math.cpp
    ${FDKAAC_DIR}/libFDK/src/huff_nodes.cpp
    ${FDKAAC_DIR}/libFDK/src/mdct.cpp
    ${FDKAAC_DIR}/libFDK/src/nlc_dec.cpp
    ${FDKAAC_DIR}/libFDK/src/qmf.cpp
    ${FDKAAC_DIR}/libFDK/src/scale.cpp)

set(MPEGTPDEC_SRC
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_adif.cpp
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_adts.cpp
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_asc.cpp
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_drm.cpp
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_latm.cpp
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_lib.cpp
    ${FDKAAC_DIR}/libMpegTPDec/src/tp_version.h
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_adif.h
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_adts.h
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_drm.h
    ${FDKAAC_DIR}/libMpegTPDec/src/tpdec_latm.h)

set(MPEGTPENC_SRC
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_adif.cpp
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_adts.cpp
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_asc.cpp
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_latm.cpp
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_lib.cpp
    ${FDKAAC_DIR}/libMpegTPEnc/src/tp_version.h
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_adif.h
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_adts.h
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_asc.h
    ${FDKAAC_DIR}/libMpegTPEnc/src/tpenc_latm.h)

set(PCMUTILS_SRC
    ${FDKAAC_DIR}/libPCMutils/src/limiter.cpp
    ${FDKAAC_DIR}/libPCMutils/src/pcm_utils.cpp
    ${FDKAAC_DIR}/libPCMutils/src/pcmdmx_lib.cpp
    ${FDKAAC_DIR}/libPCMutils/src/version.h)

set(SACDEC_SRC
    ${FDKAAC_DIR}/libSACdec/src/sac_bitdec.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_calcM1andM2.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_dec.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_dec_conceal.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_dec_lib.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_process.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_qmf.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_reshapeBBEnv.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_rom.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_smoothing.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_stp.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_tsd.cpp
    ${FDKAAC_DIR}/libSACdec/src/sac_bitdec.h
    ${FDKAAC_DIR}/libSACdec/src/sac_calcM1andM2.h
    ${FDKAAC_DIR}/libSACdec/src/sac_dec.h
    ${FDKAAC_DIR}/libSACdec/src/sac_dec_conceal.h
    ${FDKAAC_DIR}/libSACdec/src/sac_dec_interface.h
    ${FDKAAC_DIR}/libSACdec/src/sac_dec_ssc_struct.h
    ${FDKAAC_DIR}/libSACdec/src/sac_process.h
    ${FDKAAC_DIR}/libSACdec/src/sac_qmf.h
    ${FDKAAC_DIR}/libSACdec/src/sac_reshapeBBEnv.h
    ${FDKAAC_DIR}/libSACdec/src/sac_rom.h
    ${FDKAAC_DIR}/libSACdec/src/sac_smoothing.h
    ${FDKAAC_DIR}/libSACdec/src/sac_stp.h
    ${FDKAAC_DIR}/libSACdec/src/sac_tsd.h)

set(SACENC_SRC
    ${FDKAAC_DIR}/libSACenc/src/sacenc_bitstream.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_delay.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_dmx_tdom_enh.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_filter.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_framewindowing.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_huff_tab.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_lib.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_nlc_enc.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_onsetdetect.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_paramextract.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_staticgain.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_tree.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_vectorfunctions.cpp
    ${FDKAAC_DIR}/libSACenc/src/sacenc_bitstream.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_const.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_delay.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_dmx_tdom_enh.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_filter.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_framewindowing.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_huff_tab.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_nlc_enc.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_onsetdetect.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_paramextract.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_staticgain.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_tree.h
    ${FDKAAC_DIR}/libSACenc/src/sacenc_vectorfunctions.h)

set(SBRDEC_SRC
    ${FDKAAC_DIR}/libSBRdec/src/HFgen_preFlat.cpp
    ${FDKAAC_DIR}/libSBRdec/src/env_calc.cpp
    ${FDKAAC_DIR}/libSBRdec/src/env_dec.cpp
    ${FDKAAC_DIR}/libSBRdec/src/env_extr.cpp
    ${FDKAAC_DIR}/libSBRdec/src/hbe.cpp
    ${FDKAAC_DIR}/libSBRdec/src/huff_dec.cpp
    ${FDKAAC_DIR}/libSBRdec/src/lpp_tran.cpp
    ${FDKAAC_DIR}/libSBRdec/src/psbitdec.cpp
    ${FDKAAC_DIR}/libSBRdec/src/psdec.cpp
    ${FDKAAC_DIR}/libSBRdec/src/psdec_drm.cpp
    ${FDKAAC_DIR}/libSBRdec/src/psdecrom_drm.cpp
    ${FDKAAC_DIR}/libSBRdec/src/pvc_dec.cpp
    ${FDKAAC_DIR}/libSBRdec/src/sbr_deb.cpp
    ${FDKAAC_DIR}/libSBRdec/src/sbr_dec.cpp
    ${FDKAAC_DIR}/libSBRdec/src/sbr_ram.cpp
    ${FDKAAC_DIR}/libSBRdec/src/sbr_rom.cpp
    ${FDKAAC_DIR}/libSBRdec/src/sbrdec_drc.cpp
    ${FDKAAC_DIR}/libSBRdec/src/sbrdec_freq_sca.cpp
    ${FDKAAC_DIR}/libSBRdec/src/sbrdecoder.cpp
    ${FDKAAC_DIR}/libSBRdec/src/env_calc.h
    ${FDKAAC_DIR}/libSBRdec/src/env_dec.h
    ${FDKAAC_DIR}/libSBRdec/src/env_extr.h
    ${FDKAAC_DIR}/libSBRdec/src/hbe.h
    ${FDKAAC_DIR}/libSBRdec/src/HFgen_preFlat.h
    ${FDKAAC_DIR}/libSBRdec/src/huff_dec.h
    ${FDKAAC_DIR}/libSBRdec/src/lpp_tran.h
    ${FDKAAC_DIR}/libSBRdec/src/psbitdec.h
    ${FDKAAC_DIR}/libSBRdec/src/psdec.h
    ${FDKAAC_DIR}/libSBRdec/src/psdec_drm.h
    ${FDKAAC_DIR}/libSBRdec/src/pvc_dec.h
    ${FDKAAC_DIR}/libSBRdec/src/sbr_deb.h
    ${FDKAAC_DIR}/libSBRdec/src/sbr_dec.h
    ${FDKAAC_DIR}/libSBRdec/src/sbr_ram.h
    ${FDKAAC_DIR}/libSBRdec/src/sbr_rom.h
    ${FDKAAC_DIR}/libSBRdec/src/sbrdec_drc.h
    ${FDKAAC_DIR}/libSBRdec/src/sbrdec_freq_sca.h
    ${FDKAAC_DIR}/libSBRdec/src/transcendent.h)

set(SBRENC_SRC
    ${FDKAAC_DIR}/libSBRenc/src/bit_sbr.cpp
    ${FDKAAC_DIR}/libSBRenc/src/code_env.cpp
    ${FDKAAC_DIR}/libSBRenc/src/env_bit.cpp
    ${FDKAAC_DIR}/libSBRenc/src/env_est.cpp
    ${FDKAAC_DIR}/libSBRenc/src/fram_gen.cpp
    ${FDKAAC_DIR}/libSBRenc/src/invf_est.cpp
    ${FDKAAC_DIR}/libSBRenc/src/mh_det.cpp
    ${FDKAAC_DIR}/libSBRenc/src/nf_est.cpp
    ${FDKAAC_DIR}/libSBRenc/src/ps_bitenc.cpp
    ${FDKAAC_DIR}/libSBRenc/src/ps_encode.cpp
    ${FDKAAC_DIR}/libSBRenc/src/ps_main.cpp
    ${FDKAAC_DIR}/libSBRenc/src/resampler.cpp
    ${FDKAAC_DIR}/libSBRenc/src/sbr_encoder.cpp
    ${FDKAAC_DIR}/libSBRenc/src/sbr_misc.cpp
    ${FDKAAC_DIR}/libSBRenc/src/sbrenc_freq_sca.cpp
    ${FDKAAC_DIR}/libSBRenc/src/sbrenc_ram.cpp
    ${FDKAAC_DIR}/libSBRenc/src/sbrenc_rom.cpp
    ${FDKAAC_DIR}/libSBRenc/src/ton_corr.cpp
    ${FDKAAC_DIR}/libSBRenc/src/tran_det.cpp
    ${FDKAAC_DIR}/libSBRenc/src/bit_sbr.h
    ${FDKAAC_DIR}/libSBRenc/src/cmondata.h
    ${FDKAAC_DIR}/libSBRenc/src/code_env.h
    ${FDKAAC_DIR}/libSBRenc/src/env_bit.h
    ${FDKAAC_DIR}/libSBRenc/src/env_est.h
    ${FDKAAC_DIR}/libSBRenc/src/fram_gen.h
    ${FDKAAC_DIR}/libSBRenc/src/invf_est.h
    ${FDKAAC_DIR}/libSBRenc/src/mh_det.h
    ${FDKAAC_DIR}/libSBRenc/src/nf_est.h
    ${FDKAAC_DIR}/libSBRenc/src/ps_bitenc.h
    ${FDKAAC_DIR}/libSBRenc/src/ps_const.h
    ${FDKAAC_DIR}/libSBRenc/src/ps_encode.h
    ${FDKAAC_DIR}/libSBRenc/src/ps_main.h
    ${FDKAAC_DIR}/libSBRenc/src/resampler.h
    ${FDKAAC_DIR}/libSBRenc/src/sbr.h
    ${FDKAAC_DIR}/libSBRenc/src/sbr_def.h
    ${FDKAAC_DIR}/libSBRenc/src/sbr_misc.h
    ${FDKAAC_DIR}/libSBRenc/src/sbrenc_freq_sca.h
    ${FDKAAC_DIR}/libSBRenc/src/sbrenc_ram.h
    ${FDKAAC_DIR}/libSBRenc/src/sbrenc_rom.h
    ${FDKAAC_DIR}/libSBRenc/src/ton_corr.h
    ${FDKAAC_DIR}/libSBRenc/src/tran_det.h)

set(SYS_SRC
    ${FDKAAC_DIR}/libSYS/src/genericStds.cpp
    ${FDKAAC_DIR}/libSYS/src/syslib_channelMapDescr.cpp)

set(libfdk_aac_SOURCES
    ${AACDEC_SRC} ${AACENC_SRC}
    ${ARITHCODING_SRC}
    ${DRCDEC_SRC}
    ${MPEGTPDEC_SRC} ${MPEGTPENC_SRC}
    ${SACDEC_SRC} ${SACENC_SRC}
    ${SBRDEC_SRC} ${SBRENC_SRC}
    ${PCMUTILS_SRC} ${FDK_SRC} ${SYS_SRC})

set(libfdk_aac_PUB_INC
    ${FDKAAC_DIR}/libAACdec/include
    ${FDKAAC_DIR}/libAACenc/include
    ${FDKAAC_DIR}/libSYS/include
)

set(libfdk_aac_PRV_INC
    ${FDKAAC_DIR}/libArithCoding/include
    ${FDKAAC_DIR}/libDRCdec/include
    ${FDKAAC_DIR}/libSACdec/include
    ${FDKAAC_DIR}/libSACenc/include
    ${FDKAAC_DIR}/libSBRdec/include
    ${FDKAAC_DIR}/libSBRenc/include
    ${FDKAAC_DIR}/libMpegTPDec/include
    ${FDKAAC_DIR}/libMpegTPEnc/include
    ${FDKAAC_DIR}/libFDK/include
    ${FDKAAC_DIR}/libPCMutils/include
)

# setup library interface
add_library(fdk-aac INTERFACE)
target_include_directories(fdk-aac INTERFACE ${libfdk_aac_PUB_INC})

# setup shared library
add_library(fdk-aac-shared SHARED ${libfdk_aac_SOURCES})
set_target_properties(fdk-aac-shared PROPERTIES POSITION_INDEPENDENT_CODE 1)
set_target_properties(fdk-aac-shared PROPERTIES OUTPUT_NAME fdk-aac)
target_compile_options(fdk-aac-shared PRIVATE -fno-exceptions -fno-rtti)
target_include_directories(fdk-aac-shared PUBLIC ${libfdk_aac_PUB_INC} PRIVATE ${libfdk_aac_PRV_INC})

# setup static library
add_library(fdk-aac-static STATIC ${libfdk_aac_SOURCES})
set_target_properties(fdk-aac-static PROPERTIES OUTPUT_NAME fdk-aac)
target_compile_options(fdk-aac-static PRIVATE -fno-exceptions -fno-rtti)
target_include_directories(fdk-aac-static PUBLIC ${libfdk_aac_PUB_INC} PRIVATE ${libfdk_aac_PRV_INC})
