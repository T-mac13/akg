/**
 * Copyright 2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "poly/davinci_mgr_strategy.h"

#include "poly/schedule_pass/group.h"
#include "poly/schedule_pass/tile_outer_band.h"
#include "poly/schedule_pass/memory_manager.h"
#include "poly/schedule_pass/sink_c0.h"
#include "poly/schedule_pass/sink_last_axis.h"
#include "poly/schedule_pass/reorder_invariant_set_schedule.h"
#include "poly/schedule_pass/keep_outer_band_order.h"

#include "poly/schedule_pass/split_outer_band.h"
#include "poly/schedule_pass/transfer_stmt.h"
#include "poly/schedule_pass/reset_coincidence_of_reduce.h"
#include "poly/schedule_pass/set_all_coincidence.h"
#include "poly/schedule_pass/reschedule.h"
#include "poly/schedule_pass/reorder_inner_band.h"
#include "poly/schedule_pass/change_marknode_position.h"
#include "poly/schedule_pass/insert_node_for_allocc.h"
#include "poly/schedule_pass/label_realize_out_position.h"
#include "poly/schedule_pass/mark_fuse_op.h"
#include "poly/schedule_pass/reorder_mark_nodes.h"
#include "poly/schedule_pass/compute_transfer_copyin.h"
#include "poly/schedule_pass/compute_inner_band_dependency.h"
#include "poly/schedule_pass/mark_outer_most.h"

namespace akg {
namespace ir {
namespace poly {

void DavinciMgrStrategy::RegisterTilingPasses() {
  RegisterPass(std::make_shared<TileOuterBand>(pass_info_, scop_info_));
}

void DavinciMgrStrategy::RegisterMemPromPasses() { RegisterPass(std::make_shared<MemoryManager>(scop_info_)); }

void DavinciMgrStrategy::RegisterPasses() {
  passes_.clear();
  RegisterNormalizationPasses();
  if (!scop_info_.user_config_.GetDisableGroup()) {
    RegisterPass(std::make_shared<GroupStatements>(pass_info_));
  }
  RegisterSchedulingPasses();
  RegisterPass(std::make_shared<ReorderInvariantSetSchedule>(pass_info_));
  if (scop_info_.user_config_.GetReorderSchedule()) {
    RegisterPass(std::make_shared<SinkC0>());
  }
  if (scop_info_.user_config_.GetSinkLastAxis()) {
    RegisterPass(std::make_shared<SinkLastAxis>(pass_info_));
  }
  if (scop_info_.user_config_.GetKeepOuterBandOrder()) {
    RegisterPass(std::make_shared<KeepOuterBandOrder>(scop_info_));
  }
  RegisterPass(std::make_shared<UnGroupStatements>(pass_info_));
  if (scop_info_.user_config_.GetOuterBandNeedSplit() && !scop_info_.cube_info_.IsSpecGemm()) {
    RegisterPass(std::make_shared<SplitOuterBand>());
  }
  RegisterPass(std::make_shared<ComputeInnerBandDependency>(scop_info_));
  if (!scop_info_.cube_info_.IsSpecGemm() && (scop_info_.cube_info_.IsConv() || scop_info_.cube_info_.IsGemm())) {
    RegisterPass(std::make_shared<ComputeTransferCopyin>(scop_info_, pass_info_));
  }
  if (scop_info_.user_config_.GetIsTuning()) {
    return;
  }
  RegisterTilingPasses();
  RegisterPass(std::make_shared<ReorderInvariantSetSchedule>(pass_info_));
  RegisterPass(std::make_shared<ResetCoincidenceOfReduce>(scop_info_, pass_info_));
  if (scop_info_.user_config_.GetPragmaSetAllCoincident()) {
    RegisterPass(std::make_shared<SetAllCoincidence>());
  }
  if (!scop_info_.user_config_.GetIsDynamic() || !scop_info_.cube_info_.IsConv()) {
    RegisterPass(std::make_shared<Reschedule>(scop_info_, pass_info_));
  }
  RegisterPass(std::make_shared<ReorderInnerBand>(scop_info_.analysis_result_.GetCondVarsMap()));
  RegisterPass(std::make_shared<ChangeMarkNodePosition>(scop_info_.analysis_result_.ExtractWithStmtId()));
  RegisterPass(std::make_shared<LabelRealizeOutPosition>());
  if (scop_info_.cube_info_.IsSpecGemm() || scop_info_.cube_info_.IsGemm() ||
      scop_info_.cube_info_.IsConvBackpropFilter()) {
    RegisterPass(std::make_shared<InsertNodeForAllocC>());
  }
  RegisterMemPromPasses();
  if (!scop_info_.cube_info_.IsSpecGemm()) {
    RegisterPass(std::make_shared<TransferStmt>(scop_info_, pass_info_));
  }
  RegisterPass(std::make_shared<ReorderMarkNodes>());
  RegisterPass(std::make_shared<MarkFuseOp>());
  // if coincidence constraints are disabled (due to reschedule), we cannot determine multicore axis reliably
  bool can_use_multiCore = !scop_info_.cube_info_.IsSpecGemm() && scop_info_.user_config_.GetConsiderCoincidence();
  if (can_use_multiCore || scop_info_.user_config_.GetEnableMarkMultiCore()) {
    RegisterPass(std::make_shared<MarkOuterMost>(scop_info_));
  }
}

}  // namespace poly
}  // namespace ir
}  // namespace akg
