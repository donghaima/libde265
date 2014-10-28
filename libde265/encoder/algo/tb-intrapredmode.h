/*
 * H.265 video codec.
 * Copyright (c) 2013-2014 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * Authors: Dirk Farin <farin@struktur.de>
 *
 * This file is part of libde265.
 *
 * libde265 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libde265 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libde265.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TB_INTRAPREDMODE_H
#define TB_INTRAPREDMODE_H

#include "libde265/nal-parser.h"
#include "libde265/decctx.h"
#include "libde265/encoder/encode.h"
#include "libde265/slice.h"
#include "libde265/scan.h"
#include "libde265/intrapred.h"
#include "libde265/transform.h"
#include "libde265/fallback-dct.h"
#include "libde265/quality.h"
#include "libde265/fallback.h"
#include "libde265/configparam.h"


/*  Encoder search tree, bottom up:

    - Algo_TB_Split - whether TB is split or not

    - Algo_TB_IntraPredMode - choose the intra prediction mode (or NOP, if at the wrong tree level)

    - Algo_CB_IntraPartMode - choose between NxN and 2Nx2N intra parts

    - Algo_CB_Split - whether CB is split or not

    - Algo_CTB_QScale - select QScale on CTB granularity
 */


// ========== TB intra prediction mode ==========

enum ALGO_TB_IntraPredMode {
  ALGO_TB_IntraPredMode_BruteForce,
  ALGO_TB_IntraPredMode_FastBrute,
  ALGO_TB_IntraPredMode_MinResidual
};

class option_ALGO_TB_IntraPredMode : public choice_option<enum ALGO_TB_IntraPredMode>
{
 public:
  option_ALGO_TB_IntraPredMode() {
    add_choice("min-residual",ALGO_TB_IntraPredMode_MinResidual);
    add_choice("brute-force" ,ALGO_TB_IntraPredMode_BruteForce);
    add_choice("fast-brute"  ,ALGO_TB_IntraPredMode_FastBrute, true);
  }
};


enum TBBitrateEstimMethod {
  //TBBitrateEstim_AccurateBits,
  TBBitrateEstim_SSD,
  TBBitrateEstim_SAD,
  TBBitrateEstim_SATD_DCT,
  TBBitrateEstim_SATD_Hadamard
};

class option_TBBitrateEstimMethod : public choice_option<enum TBBitrateEstimMethod>
{
 public:
  option_TBBitrateEstimMethod() {
    add_choice("ssd",TBBitrateEstim_SSD);
    add_choice("sad",TBBitrateEstim_SAD);
    add_choice("satd-dct",TBBitrateEstim_SATD_DCT);
    add_choice("satd",TBBitrateEstim_SATD_Hadamard, true);
  }
};

class Algo_TB_Split;

class Algo_TB_IntraPredMode
{
 public:
  Algo_TB_IntraPredMode() : mTBSplitAlgo(NULL) { }
  virtual ~Algo_TB_IntraPredMode() { }

  virtual const enc_tb* analyze(encoder_context*,
                                context_model_table,
                                const de265_image* input,
                                const enc_tb* parent,
                                enc_cb* cb,
                                int x0,int y0, int xBase,int yBase, int log2TbSize,
                                int blkIdx,
                                int TrafoDepth, int MaxTrafoDepth, int IntraSplitFlag,
                                int qp) = 0;

  void setChildAlgo(Algo_TB_Split* algo) { mTBSplitAlgo = algo; }

 protected:
  Algo_TB_Split* mTBSplitAlgo;
};


enum ALGO_TB_IntraPredMode_Subset {
  ALGO_TB_IntraPredMode_Subset_All,
  ALGO_TB_IntraPredMode_Subset_HVPlus,
  ALGO_TB_IntraPredMode_Subset_DC,
  ALGO_TB_IntraPredMode_Subset_Planar
};

class option_ALGO_TB_IntraPredMode_Subset : public choice_option<enum ALGO_TB_IntraPredMode_Subset>
{
 public:
  option_ALGO_TB_IntraPredMode_Subset() {
    add_choice("all"   ,ALGO_TB_IntraPredMode_Subset_All, true);
    add_choice("HV+"   ,ALGO_TB_IntraPredMode_Subset_HVPlus);
    add_choice("DC"    ,ALGO_TB_IntraPredMode_Subset_DC);
    add_choice("planar",ALGO_TB_IntraPredMode_Subset_Planar);
  }
};


class Algo_TB_IntraPredMode_ModeSubset : public Algo_TB_IntraPredMode
{
 public:
  Algo_TB_IntraPredMode_ModeSubset() {
    for (int i=0;i<35;i++) {
      mPredMode_enabled[i] = true;
    }
  }

  void disableAllIntraPredModes() {
    for (int i=0;i<35;i++) {
      mPredMode_enabled[i] = false;
    }
  }

  void enableIntraPredMode(int mode, bool flag=true) {
    mPredMode_enabled[mode] = flag;
  }

 protected:
  bool mPredMode_enabled[35];
};


class Algo_TB_IntraPredMode_BruteForce : public Algo_TB_IntraPredMode_ModeSubset
{
 public:

  virtual const enc_tb* analyze(encoder_context*,
                                context_model_table,
                                const de265_image* input,
                                const enc_tb* parent,
                                enc_cb* cb,
                                int x0,int y0, int xBase,int yBase, int log2TbSize,
                                int blkIdx,
                                int TrafoDepth, int MaxTrafoDepth, int IntraSplitFlag,
                                int qp);
};


class Algo_TB_IntraPredMode_FastBrute : public Algo_TB_IntraPredMode_ModeSubset
{
 public:

  struct params
  {
    params() {
      keepNBest.set_ID("IntraPredMode-FastBrute-keepNBest");
      keepNBest.set_range(0,32);
      keepNBest.set_default(5);

      bitrateEstimMethod.set_ID("IntraPredMode-FastBrute-estimator");
    }

    option_TBBitrateEstimMethod bitrateEstimMethod;
    option_int keepNBest;
  };

  void registerParams(config_parameters& config) {
    config.add_option(&mParams.keepNBest);
    config.add_option(&mParams.bitrateEstimMethod);
  }

  void setParams(const params& p) { mParams=p; }


  virtual const enc_tb* analyze(encoder_context*,
                                context_model_table,
                                const de265_image* input,
                                const enc_tb* parent,
                                enc_cb* cb,
                                int x0,int y0, int xBase,int yBase, int log2TbSize,
                                int blkIdx,
                                int TrafoDepth, int MaxTrafoDepth, int IntraSplitFlag,
                                int qp);

 private:
  params mParams;
};


class Algo_TB_IntraPredMode_MinResidual : public Algo_TB_IntraPredMode_ModeSubset
{
 public:

  struct params
  {
    params() {
      bitrateEstimMethod.set_ID("IntraPredMode-MinResidual-estimator");
    }

    option_TBBitrateEstimMethod bitrateEstimMethod;
  };

  void setParams(const params& p) { mParams=p; }

  void registerParams(config_parameters& config) {
    config.add_option(&mParams.bitrateEstimMethod);
  }

  virtual const enc_tb* analyze(encoder_context*,
                                context_model_table,
                                const de265_image* input,
                                const enc_tb* parent,
                                enc_cb* cb,
                                int x0,int y0, int xBase,int yBase, int log2TbSize,
                                int blkIdx,
                                int TrafoDepth, int MaxTrafoDepth, int IntraSplitFlag,
                                int qp);

 private:
  params mParams;
};

#endif
